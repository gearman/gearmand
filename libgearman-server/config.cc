/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2013 Data Differential, http://datadifferential.com/
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
#include "libgearman-server/common.h"

#include <memory>

gearmand_config_st *gearmand_config_create(void)
{
  return new (std::nothrow) gearmand_config_st;
}

void gearmand_config_free(gearmand_config_st *config)
{
  if (config)
  {
    delete config;
  }
}

void gearmand_config_sockopt_keepalive(gearmand_config_st *config, bool keepalive_)
{
  if (config)
  {
    config->config.sockopt().keepalive(keepalive_);
  }
}

void gearmand_config_sockopt_keepalive_idle(gearmand_config_st *config, int keepalive_idle_)
{
  if (config)
  {
    config->config.sockopt().keepalive_idle(keepalive_idle_);
  }
}

void gearmand_config_sockopt_keepalive_interval(gearmand_config_st *config, int keepalive_interval_)
{
  if (config)
  {
    config->config.sockopt().keepalive_interval(keepalive_interval_);
  }
}

void gearmand_config_sockopt_keepalive_count(gearmand_config_st *config, int keepalive_count_)
{
  if (config)
  {
    config->config.sockopt().keepalive_count(keepalive_count_);
  }
}
