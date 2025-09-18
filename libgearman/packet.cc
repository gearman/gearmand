/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
 *  Copyright (C) 2008 Brian Aker, Eric Day
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following disclaimer
 *  in the documentation and/or other materials provided with the
 *  distribution.
 *
 *      * The names of its contributors may not be used to endorse or
 *  promote products derived from this software without specific prior
 *  written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * @file
 * @brief Packet Definitions
 */

#include "gear_config.h"
#include <libgearman/common.h>

#include <libgearman/allocator.hpp>
#include <libgearman/universal.hpp>
#include <libgearman/command.h>
#include <libgearman/packet.hpp>

#include <libgearman/backtrace.hpp>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>

/**
 * @addtogroup gearman_packet_static Static Packet Declarations
 * @ingroup gearman_packet
 * @{
 */

#pragma GCC diagnostic push
#ifndef __INTEL_COMPILER
# pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
inline static gearman_return_t packet_create_arg(gearman_packet_st *packet,
                                                 const void *arg, size_t arg_size)
{
  if (packet->argc == gearman_command_info(packet->command)->argc and
      (not (gearman_command_info(packet->command)->data) || packet->data != nullptr))
  {
    return gearman_universal_set_error(*packet->universal, GEARMAN_TOO_MANY_ARGS, GEARMAN_AT, "too many arguments for command (%s)",
                                       gearman_command_info(packet->command)->name);
  }

  if (packet->argc == gearman_command_info(packet->command)->argc)
  {
    if (gearman_command_info(packet->command)->data)
    {
      if (packet->universal->options.no_new_data)
      {
        packet->data= arg;
        packet->data_size= arg_size;
      } 
      else
      {
        packet->data= gearman_malloc(*packet->universal, arg_size);
        if (packet->data == nullptr)
        {
          return gearman_perror(*packet->universal, errno, "packet->data");
        }

        memcpy((void*)packet->data, arg, arg_size);
        packet->data_size= arg_size;
        packet->options.free_data= true;
      }

      return GEARMAN_SUCCESS;
    }
  }

  if (packet->args_size == 0 and packet->magic != GEARMAN_MAGIC_TEXT)
  {
    packet->args_size= GEARMAN_PACKET_HEADER_SIZE;
  }

  if ((packet->args_size + arg_size) < GEARMAN_ARGS_BUFFER_SIZE)
  {
    packet->args= packet->args_buffer;
  }
  else
  {
    bool was_args_buffer{false};
    // If args is args_buffer we don't want to try realloc it
    if (packet->args == packet->args_buffer)
    {
      was_args_buffer= true;
      packet->args= nullptr;
    }

    char *new_args= static_cast<char *>(realloc(packet->args, packet->args_size + arg_size +1));
    if (new_args == nullptr)
    {
      return gearman_perror(*packet->universal, errno, "packet realloc");
    }

    if (was_args_buffer and packet->args_size > 0)
    {
      memcpy(new_args, packet->args_buffer, packet->args_size);
    }

    packet->args= new_args;
  }

  memcpy(packet->args + packet->args_size, arg, arg_size);
  packet->args_size+= arg_size;
  packet->arg_size[packet->argc]= arg_size;
  packet->argc++;

  size_t offset{};
  if (packet->magic == GEARMAN_MAGIC_TEXT)
  {
    offset= 0;
  }
  else
  {
    offset= GEARMAN_PACKET_HEADER_SIZE;
  }

  for (uint8_t x= 0; x < packet->argc; x++)
  {
    packet->arg[x]= packet->args + offset;
    offset+= packet->arg_size[x];
  }

  return GEARMAN_SUCCESS;
}
#pragma GCC diagnostic pop

/** @} */

/*
 * Public Definitions
 */

#ifdef GEARMAN_PACKET_TRACE
#include <pthread.h>
pthread_mutex_t mutex= PTHREAD_MUTEX_INITIALIZER;
static uint32_t global_packet_id= 0;
#endif

gearman_packet_st *gearman_packet_create(gearman_universal_st &universal,
                                         gearman_packet_st& packet)
{
  packet.reset();
  packet.universal= &universal;

#ifdef GEARMAN_PACKET_TRACE
  pthread_mutex_lock(&mutex);
  packet->_id= global_packet_id++;
  pthread_mutex_unlock(&mutex);
  fprintf(stderr, "%s PACKET %u\n", __func__, packet->_id);
  custom_backtrace();
#endif

  // dont_track_packets == false
  {
    if (universal.packet_list != nullptr)
    {
      universal.packet_list->prev= &packet;
    }
    packet.next= universal.packet_list;
    packet.prev= nullptr;
    universal.packet_list= &packet;
    universal.packet_count++;
  }

  return &packet;
}

gearman_return_t gearman_packet_create_arg(gearman_packet_st& self,
                                           const void *arg, size_t arg_size)
{
  return packet_create_arg(&self, arg, arg_size);
}

gearman_return_t gearman_packet_create_args(gearman_universal_st& universal,
                                            gearman_packet_st& packet,
                                            enum gearman_magic_t magic,
                                            gearman_command_t command,
                                            const void *args[],
                                            const size_t args_size[],
                                            size_t args_count)
{
  gearman_packet_create(universal, packet);

  packet.magic= magic;
  packet.command= command;

  for (size_t x= 0; x < args_count; x++)
  {
    gearman_return_t ret= packet_create_arg(&packet, args[x], args_size[x]);
    if (gearman_failed(ret))
    {
      assert(ret == universal.error_code());
      gearman_packet_free(&packet);
      return universal.error_code();
    }
  }

  gearman_return_t ret= gearman_packet_pack_header(&packet);
  if (gearman_failed(ret))
  {
    assert(ret == universal.error_code());
    gearman_packet_free(&packet);
    return universal.error_code();
  }

  return ret;
}

void gearman_packet_free(gearman_packet_st *packet)
{
#ifdef GEARMAN_PACKET_TRACE
  fprintf(stderr, "%s PACKET %u\n", __func__, packet->_id);
  custom_backtrace();
#endif

  assert_msg(packet->universal, 
             "Packet that is being freed has not been allocated, most likely this is due to freeing a gearman_task_st or other object twice");

  if (gearman_is_allocated(packet))
  {
    delete packet;
  }
  else
  {
    packet->reset();
  }
}

gearman_return_t gearman_packet_pack_header(gearman_packet_st *packet)
{
  if (packet->magic == GEARMAN_MAGIC_TEXT)
  {
    packet->options.complete= true;
    return GEARMAN_SUCCESS;
  }

  if (packet->args_size == 0)
  {
    packet->args= packet->args_buffer;
    packet->args_size= GEARMAN_PACKET_HEADER_SIZE;
  }

  switch (packet->magic)
  {
  case GEARMAN_MAGIC_TEXT:
    break;

  case GEARMAN_MAGIC_REQUEST:
    memcpy(packet->args, "\0REQ", 4);
    break;

  case GEARMAN_MAGIC_RESPONSE:
    memcpy(packet->args, "\0RES", 4);
    break;

  default:
    return gearman_error(*packet->universal, GEARMAN_INVALID_MAGIC, "invalid magic value");
  }

  if (packet->command == GEARMAN_COMMAND_TEXT ||
      packet->command >= GEARMAN_COMMAND_MAX)
  {
    return gearman_error(*packet->universal, GEARMAN_INVALID_COMMAND, "invalid command value");
  }

  uint32_t tmp= packet->command;
  tmp= htonl(tmp);
  // Record the command
  memcpy(packet->args + 4, &tmp, 4);

  uint64_t length_64= packet->args_size + packet->data_size - GEARMAN_PACKET_HEADER_SIZE;

  // Check for overflow on 32bit(portable?).
  if (length_64 >= UINT32_MAX || length_64 < packet->data_size)
  {
    return gearman_error(*packet->universal, GEARMAN_ARGUMENT_TOO_LARGE, "data size too too long");
  }

  tmp= uint32_t(length_64);
  tmp= htonl(tmp);
  // Record the length of the packet
  memcpy(packet->args + 8, &tmp, 4);

  packet->options.complete= true;

  return GEARMAN_SUCCESS;
}

gearman_return_t gearman_packet_unpack_header(gearman_packet_st *packet)
{
  uint32_t tmp{};

  if (not memcmp(packet->args, "\0REQ", 4))
  {
    packet->magic= GEARMAN_MAGIC_REQUEST;
  }
  else if (not memcmp(packet->args, "\0RES", 4))
  {
    packet->magic= GEARMAN_MAGIC_RESPONSE;
  }
  else
  {
    gearman_error(*packet->universal, GEARMAN_INVALID_MAGIC, "invalid magic value");
    return GEARMAN_INVALID_MAGIC;
  }

  memcpy(&tmp, packet->args + 4, 4);
  packet->command= (gearman_command_t)ntohl(tmp);

  if (packet->command == GEARMAN_COMMAND_TEXT ||
      packet->command >= GEARMAN_COMMAND_MAX)
  {
    return gearman_error(*packet->universal, GEARMAN_INVALID_COMMAND, "invalid command value");
  }

  memcpy(&tmp, packet->args + 8, 4);
  packet->data_size= ntohl(tmp);

  return GEARMAN_SUCCESS;
}

size_t gearman_packet_pack(const gearman_packet_st &self,
                           void *data, size_t data_size,
                           gearman_return_t &ret)
{
  ret= GEARMAN_SUCCESS;

  if (self.args_size == 0)
  {
    return 0;
  }

  if (self.args_size > data_size)
  {
    ret= GEARMAN_FLUSH_DATA;
    return 0;
  }

  memcpy(data, self.args, self.args_size);

  return self.args_size;
}

size_t gearman_packet_unpack(gearman_packet_st& self,
                             const void *data, size_t data_size,
                             gearman_return_t &ret)
{
  size_t used_size{};
  size_t arg_size{};

  if (self.args_size == 0)
  {
    if (data_size > 0 && ((char *)data)[0] != 0)
    {
      /* Try to parse a text-based command. */
      char *ptr= (char *)memchr(data, '\n', data_size);
      if (!ptr)
      {
        ret= gearman_gerror(*self.universal, GEARMAN_IO_WAIT);
        return 0;
      }

      self.magic= GEARMAN_MAGIC_TEXT;
      self.command= GEARMAN_COMMAND_TEXT;

      used_size= (size_t)(ptr - ((char *)data)) + 1;
      *ptr= 0;
      if (used_size > 1 && *(ptr - 1) == '\r')
      {
        *(ptr - 1)= 0;
      }

      for (arg_size= used_size, ptr= (char *)data; ptr != nullptr; data= ptr)
      {
        ptr= (char *)memchr(data, ' ', arg_size);
        if (!ptr)
        {
          *ptr= 0;
          ptr++;
          while (*ptr == ' ')
          {
            ptr++;
          }

          arg_size-= (size_t)(ptr - ((char *)data));
        }

        ret= packet_create_arg(&self, data, 
                               ptr == nullptr ? arg_size : size_t(ptr - ((char *)data)));
        if (gearman_failed(ret))
        {
          return used_size;
        }
      }

      return used_size;
    }
    else if (data_size < GEARMAN_PACKET_HEADER_SIZE)
    {
      ret= gearman_gerror(*self.universal, GEARMAN_IO_WAIT);
      return 0;
    }

    self.args= self.args_buffer;
    self.args_size= GEARMAN_PACKET_HEADER_SIZE;
    memcpy(self.args, data, GEARMAN_PACKET_HEADER_SIZE);

    ret= gearman_packet_unpack_header(&self);
    if (gearman_failed(ret))
    {
      return 0;
    }

    used_size= GEARMAN_PACKET_HEADER_SIZE;
  }
  else
  {
    used_size= 0;
  }

  while (self.argc != gearman_command_info(self.command)->argc)
  {
    char *location= (char *)data +used_size;

    if (self.argc != (gearman_command_info(self.command)->argc - 1) or
        gearman_command_info(self.command)->data)
    {
      void *ptr= memchr(location, 0, data_size - used_size);
      if (!ptr)
      {
        ret= gearman_gerror(*self.universal, GEARMAN_IO_WAIT);
        return used_size;
      }

      arg_size= size_t((char*)ptr -location) +1;
      ret= packet_create_arg(&self, location, arg_size);
      if (gearman_failed(ret))
      {
        return used_size;
      }

      self.data_size-= arg_size;
      used_size+= arg_size;
    }
    else
    {
      if ((data_size - used_size) < self.data_size)
      {
        ret= gearman_gerror(*self.universal, GEARMAN_IO_WAIT);
        return used_size;
      }

      ret= packet_create_arg(&self, location, self.data_size);
      if (gearman_failed(ret))
      {
        return used_size;
      }

      used_size+= self.data_size;
      self.data_size= 0;
    }
  }

  ret= GEARMAN_SUCCESS;
  return used_size;
}

void gearman_packet_give_data(gearman_packet_st& self,
                              const void *data, size_t data_size)
{
  self.data= data;
  self.data_size= data_size;
  self.options.free_data= true;
}

void *gearman_packet_take_data(gearman_packet_st& self, size_t *data_size)
{
  void *data= const_cast<void *>(self.data);

  *data_size= self.data_size;

  self.data= nullptr;
  self.data_size= 0;
  self.options.free_data= false;

  return data;
}

void gearman_packet_st::free__data()
{
  if (universal and options.free_data and data)
  {
    void* tmp= (void*)data;
    gearman_free((*universal), tmp);
    data= nullptr;
    options.free_data= false;
  }
}

void gearman_packet_st::reset()
{
  if (args != args_buffer and args)
  {
    // Created with realloc
    free(args);
    args= nullptr;
  }

  free__data();

  if (universal and universal->packet_list)
  {
    if (universal->packet_list == this)
    {
      universal->packet_list= next;
    }

    if (prev)
    {
      prev->next= next;
    }

    if (next)
    {
      next->prev= prev;
    }

    universal->packet_count--;
  }

  options.complete= false;
  options.free_data= false;
  magic= GEARMAN_MAGIC_TEXT;
  command= GEARMAN_COMMAND_TEXT;
  argc= 0;
  args_size= 0;
  data_size= 0;
  universal= nullptr;
  next= 0;
  prev= 0;
  args= 0;
  data= 0;
}
