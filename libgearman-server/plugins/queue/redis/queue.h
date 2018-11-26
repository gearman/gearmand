/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
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

#pragma once

#if defined(HAVE_HIREDIS) && HAVE_HIREDIS
#ifndef GEARMAND_PLUGINS_QUEUE_REDIS_H
#define GEARMAND_PLUGINS_QUEUE_REDIS_H

#include <libgearman-server/common.h>
#include <libgearman-server/plugins/queue/base.h>
#include <hiredis/hiredis.h>

typedef std::vector<char> vchar_t;

namespace gearmand {
namespace plugins {
namespace queue {

/**
 * redis result record struct
 */
struct redis_record_t {
  uint32_t priority;
  std::string data;
};

/**
 * redis queue class
 * provides redisContex object
 * and a set of helper methods
 */
class Hiredis : public Queue {
  private:
    redisContext *_redis;
  public:
    std::string server;
    std::string service;
    std::string password;
    std::string prefix;

    Hiredis();
    ~Hiredis();

    gearmand_error_t initialize();

    redisContext* redis();

    /*
     * hmset(vchar_t key, const void *data, size_t data_size, uint32_t)
     *
     * returns true if hiredis HMSET succeeded
     */
    bool hmset(vchar_t, const void *, size_t, uint32_t);

    /*
     * bool fetch(char *key, redis_record_t &req)
     *
     * fetch redis data for the key
     * and put it into record
     *
     * returns true on success
     */
    bool fetch(char *, redis_record_t &);
}; // class Hiredis

void initialize_redis();

} // namespace queue
} // namespace plugins
} // namespace gearmand

#endif // ifndef GEARMAND_PLUGINS_QUEUE_REDIS_H
#endif // if defined(HAVE_HIREDIS) && HAVE_HIREDIS
