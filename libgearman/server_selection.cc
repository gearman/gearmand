/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2026 Data Differential, http://datadifferential.com/
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

#include "gear_config.h"
#include <libgearman/common.h>

#include "libgearman/server_selection.hpp"

#include <libhashkit-1.0/hashkit.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

// Classic ketama layout: each server gets a handful of replica points on the
// ring, and each replica's md5 digest is split into four 32-bit points, to
// smooth out the distribution without hashing every server hundreds of
// times.
const uint32_t KETAMA_REPLICAS_PER_SERVER= 4;

struct KetamaPoint
{
  uint32_t point;
  gearman_connection_st* con;

  bool operator<(const KetamaPoint& rhs) const
  {
    return point < rhs.point;
  }
};

void build_continuum(gearman_universal_st& universal, std::vector<KetamaPoint>& continuum)
{
  for (gearman_connection_st* con= universal.con_list; con; con= con->next_connection())
  {
    for (uint32_t replica= 0; replica < KETAMA_REPLICAS_PER_SERVER; replica++)
    {
      char point_key[GEARMAN_NI_MAXHOST + GEARMAN_NI_MAXSERV + 16];
      int point_key_length= snprintf(point_key, sizeof(point_key), "%s:%s-%u",
                                     con->host(), con->service(), replica);
      if (point_key_length <= 0)
      {
        continue;
      }

      unsigned char digest[16];
      libhashkit_md5_signature(reinterpret_cast<const unsigned char*>(point_key),
                               size_t(point_key_length), digest);

      for (int chunk= 0; chunk < 4; chunk++)
      {
        uint32_t point=
          (uint32_t(digest[chunk * 4 + 3]) << 24) |
          (uint32_t(digest[chunk * 4 + 2]) << 16) |
          (uint32_t(digest[chunk * 4 + 1]) << 8)  |
           uint32_t(digest[chunk * 4 + 0]);

        KetamaPoint entry= { point, con };
        continuum.push_back(entry);
      }
    }
  }

  std::sort(continuum.begin(), continuum.end());
}

} // namespace

gearman_connection_st* client_select_connection_by_key(gearman_universal_st& universal,
                                                        const char* key,
                                                        size_t key_length,
                                                        gearman_connection_st* after_con)
{
  std::vector<KetamaPoint> continuum;
  build_continuum(universal, continuum);

  if (continuum.empty())
  {
    return NULL;
  }

  size_t start_index;

  if (after_con)
  {
    size_t after_index= 0;
    for (size_t i= 0; i < continuum.size(); i++)
    {
      if (continuum[i].con == after_con)
      {
        after_index= i;
        break;
      }
    }
    start_index= (after_index + 1) % continuum.size();
  }
  else
  {
    uint32_t hash= libhashkit_md5(key, key_length);

    size_t low= 0, high= continuum.size();
    while (low < high)
    {
      size_t mid= low + (high - low) / 2;
      if (continuum[mid].point < hash)
      {
        low= mid + 1;
      }
      else
      {
        high= mid;
      }
    }
    start_index= (low == continuum.size()) ? 0 : low;
  }

  for (size_t seen= 0; seen < continuum.size(); seen++)
  {
    gearman_connection_st* con= continuum[(start_index + seen) % continuum.size()].con;

    if (after_con and con == after_con)
    {
      continue;
    }

    if (con->send_state == GEARMAN_CON_SEND_STATE_NONE)
    {
      return con;
    }
  }

  return NULL;
}
