/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Cycle the Gearmand server
 *
 *  Copyright (C) 2013 Data Differential, http://datadifferential.com/
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


/*
  Test that we are cycling the servers we are creating during testing.
*/

#include "gear_config.h"

#include <libtest/test.hpp>
using namespace libtest;

#include "libgearman/vector.hpp"
#include "libgearman/vector.h"

static test_return_t declare_vector_TEST(void*)
{
  gearman_vector_st vec;
  ASSERT_EQ(0, vec.size());
  ASSERT_EQ(0, vec.capacity());

  return TEST_SUCCESS;
}

static test_return_t new_vector_TEST(void*)
{
  gearman_vector_st* vec= new gearman_vector_st;
  ASSERT_EQ(0, vec->size());
  ASSERT_EQ(0, vec->capacity());

  delete vec;

  return TEST_SUCCESS;
}

static test_return_t declare_vector_size_TEST(void*)
{
  gearman_vector_st vec(2048);
  ASSERT_EQ(0, vec.size());
  ASSERT_TRUE(vec.capacity() >= 2048);

  return TEST_SUCCESS;
}

static test_return_t new_vector_size_TEST(void*)
{
  gearman_vector_st* vec= new gearman_vector_st(2023);

  ASSERT_EQ(0, vec->size());
  ASSERT_TRUE(vec->capacity() >= 2023);

  delete vec;

  return TEST_SUCCESS;
}

static test_return_t gearman_string_create_NULL_NULL_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_string_create(NULL, NULL, 0));

  return TEST_SUCCESS;
}

static test_return_t gearman_string_create_TEST(void*)
{
  gearman_vector_st* vec= gearman_string_create(NULL, 0);

  ASSERT_EQ(0, vec->size());
  ASSERT_EQ(0, vec->capacity());

  gearman_string_free(vec);

  return TEST_SUCCESS;
}

static test_return_t gearman_string_create_string_redo_size_TEST(void*)
{
  gearman_vector_st* vec= gearman_string_create(NULL, test_literal_param(__func__));

  ASSERT_EQ(test_literal_param_size(__func__), vec->size());
  ASSERT_TRUE(vec->capacity() >= test_literal_param_size(__func__));

  vec= gearman_string_create(vec, test_literal_param("again"));

  ASSERT_EQ(test_literal_param_size("again"), vec->size());
  ASSERT_TRUE(vec->capacity() >= test_literal_param_size("again"));

  gearman_string_free(vec);

  return TEST_SUCCESS;
}

static test_return_t gearman_string_create_string_size_TEST(void*)
{
  gearman_vector_st* vec= gearman_string_create(NULL, test_literal_param(__func__));

  ASSERT_EQ(test_literal_param_size(__func__), vec->size());
  ASSERT_TRUE(vec->capacity() >= test_literal_param_size(__func__));

  gearman_string_free(vec);

  return TEST_SUCCESS;
}

static test_return_t gearman_string_create_size_TEST(void*)
{
  gearman_vector_st* vec= gearman_string_create(NULL, 2023);

  ASSERT_EQ(0, vec->size());
  ASSERT_TRUE(vec->capacity() >= 2023);

  gearman_string_free(vec);

  return TEST_SUCCESS;
}

static test_return_t gearman_string_create_nonnull_size_TEST(void*)
{
  gearman_vector_st vec(2048);
  gearman_vector_st* vec_ptr= gearman_string_create(&vec, 0);
  ASSERT_EQ(0, vec.size());
  ASSERT_EQ(0, vec.capacity());

  gearman_string_free(vec_ptr);

  return TEST_SUCCESS;
}

static test_return_t zero_resize_TEST(void*)
{
  gearman_vector_st vec(89);
  ASSERT_EQ(0, vec.size());
  ASSERT_TRUE(vec.capacity() >= 89);

  vec.resize(0);
  ASSERT_EQ(0, vec.size());
  ASSERT_EQ(0, vec.capacity());

  return TEST_SUCCESS;
}

static test_return_t smaller_resize_TEST(void*)
{
  gearman_vector_st vec(89);
  ASSERT_EQ(0, vec.size());
  ASSERT_TRUE(vec.capacity() >= 89);

  vec.resize(20);
  ASSERT_EQ(0, vec.size());
  ASSERT_TRUE(vec.capacity() >= 20);

  return TEST_SUCCESS;
}

static test_return_t bigger_resize_TEST(void*)
{
  gearman_vector_st vec(89);
  ASSERT_EQ(0, vec.size());
  ASSERT_TRUE(vec.capacity() >= 89);

  vec.resize(181);
  ASSERT_EQ(0, vec.size());
  ASSERT_TRUE(vec.capacity() >= 181);

  return TEST_SUCCESS;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
static test_return_t MAX_resize_TEST(void*)
{
  const size_t max_block= 10 * GEARMAN_VECTOR_BLOCK_SIZE;
  for (size_t x= 0; x < 20; x++)
  {
    gearman_vector_st vec(random() % max_block);
    ASSERT_FALSE(vec.resize(std::numeric_limits<size_t>::max() -1));
  }

  return TEST_SUCCESS;
}
#pragma GCC diagnostic pop

static test_return_t random_resize_TEST(void*)
{
  const size_t max_block= 10 * GEARMAN_VECTOR_BLOCK_SIZE;
  for (size_t x= 0; x < 20; x++)
  {
    gearman_vector_st vec(labs(random()) % max_block);
    ASSERT_TRUE(vec.capacity() >= vec.size());

    vec.resize(labs(random()) % max_block);
    ASSERT_TRUE(vec.capacity() >= vec.size());

    vec.resize(labs(random()) % max_block +GEARMAN_VECTOR_BLOCK_SIZE);
    ASSERT_TRUE(vec.capacity() >= vec.size());
  }

  return TEST_SUCCESS;
}

static test_return_t gearman_vector_st__vec_printf_TEST(void*)
{
  for (size_t x= 0; x < 20; x++)
  {
    gearman_vector_st vec;
    vec.vec_printf("%s", __func__);
    ASSERT_EQ(0, strcmp(__func__, vec.value()));
    ASSERT_EQ(strlen(__func__), vec.size());
  }

  return TEST_SUCCESS;
}

static test_return_t gearman_vector_st__vec_printf_empty_TEST(void*)
{
  const size_t max_block= 10 * GEARMAN_VECTOR_BLOCK_SIZE;

  libtest::vchar_t base_string;
  libtest::vchar::make(base_string, random() % max_block);

  gearman_vector_st base_vec(base_string.size());
  ASSERT_TRUE(base_vec.store(vchar_param(base_string)));

  gearman_vector_st vec;
  ASSERT_TRUE(vec.store(base_vec));

  vec.vec_printf("");
  ASSERT_EQ(0, vec.size());

  return TEST_SUCCESS;
}

static test_return_t gearman_vector_st__vec_append_printf_NULL_TEST(void*)
{
  const size_t max_block= 10 * GEARMAN_VECTOR_BLOCK_SIZE;

  libtest::vchar_t base_string;
  libtest::vchar::make(base_string, random() % max_block);

  gearman_vector_st base_vec(base_string.size());
  ASSERT_TRUE(base_vec.store(vchar_param(base_string)));

  gearman_vector_st vec;
  ASSERT_TRUE(vec.store(base_vec));
  ASSERT_EQ(vec.size(), base_vec.size());
  
  // We are just testing to see if we set off NULL
  vec.vec_append_printf("%s", NULL);
  ASSERT_TRUE(vec.size() >= base_vec.size());

  return TEST_SUCCESS;
}

static test_return_t gearman_vector_st__vec_append_printf_empty_TEST(void*)
{
  const size_t max_block= 10 * GEARMAN_VECTOR_BLOCK_SIZE;

  libtest::vchar_t base_string;
  libtest::vchar::make(base_string, random() % max_block);

  gearman_vector_st base_vec(base_string.size());

  ASSERT_TRUE(base_vec.store(vchar_param(base_string)));
  gearman_vector_st vec;
  ASSERT_TRUE(vec.store(base_vec));
  ASSERT_EQ(vec.size(), base_vec.size());

  vec.vec_append_printf("");
  ASSERT_EQ(vec.size(), base_vec.size());

  return TEST_SUCCESS;
}

static test_return_t gearman_vector_st__vec_printf_NULL_TEST(void*)
{
  gearman_vector_st vec;
  
  // We are just testing to see if we set off NULL
  vec.vec_printf("%s", NULL);

  return TEST_SUCCESS;
}

static test_return_t gearman_vector_st__vec_append_printf_TEST(void*)
{
  const size_t max_block= 10 * GEARMAN_VECTOR_BLOCK_SIZE;

  libtest::vchar_t base_string;
  libtest::vchar::make(base_string, random() % max_block);

  gearman_vector_st base_vec(base_string.size());
  ASSERT_TRUE(base_vec.store(vchar_param(base_string)));

  for (size_t x= 0; x < 20; x++)
  {
    gearman_vector_st vec;
    ASSERT_TRUE(vec.store(base_vec));

    vec.vec_printf("%s", __func__);
    ASSERT_EQ(0, strcmp(__func__, vec.value()));
    ASSERT_EQ(strlen(__func__), vec.size());
  }

  return TEST_SUCCESS;
}

static test_return_t gearman_vector_st__vec_append_printf_1_TEST(void*)
{
  gearman_vector_st vec;

  for (size_t x= 0; x < 10000; x++)
  {
    vec.vec_append_printf("a");
    ASSERT_EQ(x +1, vec.size());
  }

  return TEST_SUCCESS;
}

static test_return_t gearman_vector_st__vec_printf_string2_TEST(void*)
{
  const size_t max_block= 10 * GEARMAN_VECTOR_BLOCK_SIZE;

  libtest::vchar_t base_string;
  libtest::vchar::make(base_string, random() % max_block);

  gearman_vector_st base_vec(base_string.size());
  ASSERT_TRUE(base_vec.store(vchar_param(base_string)));

  for (size_t x= 0; x < 20; x++)
  {
    gearman_vector_st vec;
    ASSERT_TRUE(vec.store(base_vec));

    libtest::vchar_t random_string1;
    libtest::vchar::make(random_string1, random() % max_block);

    libtest::vchar_t random_string2;
    libtest::vchar::make(random_string2, random() % max_block);

    // We are just testing to see if we set off NULL
    int length= vec.vec_printf("%.*s %.*s", vchar_printf(random_string1), vchar_printf(random_string2));

    ASSERT_EQ(length, int(random_string1.size() +random_string2.size() +1));
  }

  return TEST_SUCCESS;
}

static test_return_t gearman_vector_st__vec_append_printf_string2_TEST(void*)
{
  const size_t max_block= 10 * GEARMAN_VECTOR_BLOCK_SIZE;

  libtest::vchar_t base_string;
  libtest::vchar::make(base_string, random() % max_block);

  gearman_vector_st base_vec(base_string.size());
  ASSERT_TRUE(base_vec.store(vchar_param(base_string)));

  for (size_t x= 0; x < 20; x++)
  {
    gearman_vector_st vec;
    ASSERT_TRUE(vec.store(base_vec));

    libtest::vchar_t random_string1;
    libtest::vchar::make(random_string1, random() % max_block);

    libtest::vchar_t random_string2;
    libtest::vchar::make(random_string2, random() % max_block);

    // We are just testing to see if we set off NULL
    int length= vec.vec_append_printf("%.*s %.*s", vchar_printf(random_string1), vchar_printf(random_string2));
    ASSERT_TRUE(length > 0);

    ASSERT_EQ(vec.size(), size_t(random_string1.size() +random_string2.size() +base_vec.size() +1));
  }

  return TEST_SUCCESS;
}

static test_return_t gearman_vector_st__append_character_TEST(void*)
{
  for (size_t x= 0; x < 20; x++)
  {
    char test_string[3];

    gearman_vector_st vec;
    int a= libtest::random_alpha_num();
    vec.append_character(a);
    int b= libtest::random_alpha_num();
    vec.append_character(b);

    test_string[0]= a;
    test_string[1]= b;
    test_string[2]= 0;

    ASSERT_EQ(0, strcmp(test_string, vec.value()));
  }

  return TEST_SUCCESS;
}

static test_return_t gearman_string_append_character_TEST(void*)
{
  const size_t max_block= 10 * GEARMAN_VECTOR_BLOCK_SIZE;
  for (size_t x= 0; x < 20; x++)
  {
    gearman_vector_st vec(random() % max_block);
    ASSERT_TRUE(vec.capacity() >= vec.size());
    gearman_string_append_character(&vec, libtest::random_alpha_num());
    ASSERT_TRUE(vec.capacity() >= vec.size());

    if (random() % 2)
    {
      vec.clear();
      ASSERT_TRUE(vec.capacity() >= vec.size());
    }
  }

  return TEST_SUCCESS;
}

static test_return_t gearman_string_append_TEST(void*)
{
  const size_t max_block= 10 * GEARMAN_VECTOR_BLOCK_SIZE;
  for (size_t x= 0; x < 20; x++)
  {
    gearman_vector_st vec(random() % max_block);
    libtest::vchar_t random_string;
    libtest::vchar::make(random_string, random() % max_block);
    gearman_string_append(&vec, &random_string[0], random_string.size());

    if (random() % 2)
    {
      vec.clear();
    }
  }

  return TEST_SUCCESS;
}

static test_return_t gearman_string_take_TEST(void*)
{
  const size_t max_block= 10 * GEARMAN_VECTOR_BLOCK_SIZE;
  for (size_t x= 0; x < 20; x++)
  {
    gearman_vector_st vec((random() % max_block) +1);

    // Now we insert a random string
    libtest::vchar_t random_string;
    libtest::vchar::make(random_string, (random() % max_block) +1);
    gearman_string_append(&vec, &random_string[0], random_string.size());

    gearman_string_t temp= gearman_string_take_string(&vec);
    ASSERT_TRUE(gearman_c_str(temp));
    free((void*)(gearman_c_str(temp)));
  }

  return TEST_SUCCESS;
}

static test_return_t gearman_string_allocate_take_TEST(void*)
{
  const size_t max_block= 10 * GEARMAN_VECTOR_BLOCK_SIZE;
  for (size_t x= 0; x < 20; x++)
  {
    gearman_vector_st* vec= new gearman_vector_st((random() % max_block) +1);

    { // Now we insert a random string
      libtest::vchar_t random_string;
      libtest::vchar::make(random_string, random() % max_block);
      gearman_string_append(vec, &random_string[0], random_string.size());
    }

    gearman_string_t temp= gearman_string_take_string(vec);
    ASSERT_TRUE(gearman_c_str(temp));
    free((void*)(gearman_c_str(temp)));

    if (random() % 2)
    { // Now we insert a random string
      libtest::vchar_t random_string;
      libtest::vchar::make(random_string, random() % max_block);
      gearman_string_append(vec, &random_string[0], random_string.size());
    }

    delete vec;
  }

  return TEST_SUCCESS;
}

test_st allocate_TESTS[] ={
  { "declare vector", 0, declare_vector_TEST },
  { "new vector", 0, new_vector_TEST },
  { "declare vector(2048)", 0, declare_vector_size_TEST },
  { "new vector(2023)", 0, new_vector_size_TEST },
  { "gearman_string_create(NULL, 0)", 0, gearman_string_create_TEST },
  { "gearman_string_create(NULL, NULL, 0)", 0, gearman_string_create_NULL_NULL_TEST },
  { "gearman_string_create(NULL, literal)", 0, gearman_string_create_string_size_TEST },
  { "gearman_string_create(NULL, literal)x2", 0, gearman_string_create_string_redo_size_TEST },
  { "gearman_string_create(NULL, 2023)", 0, gearman_string_create_size_TEST },
  { "gearman_string_create(vec, 2023)", 0, gearman_string_create_nonnull_size_TEST },
  { 0, 0, 0 }
};

test_st resize_TESTS[] ={
  { "zero", 0, zero_resize_TEST },
  { "smaller", 0, smaller_resize_TEST },
  { "bigger", 0, bigger_resize_TEST },
  { "random", 0, random_resize_TEST },
  { "INT64_MAX", 0, MAX_resize_TEST },
  { 0, 0, 0 }
};

test_st append_TESTS[] ={
  { "gearman_string_append_character()", 0, gearman_string_append_character_TEST },
  { "gearman_string_append()", 0, gearman_string_append_TEST },
  { "gearman_vector_st::append_character()", 0, gearman_vector_st__append_character_TEST },
  { 0, 0, 0 }
};

test_st take_TESTS[] ={
  { "gearman_string_take_string()", 0, gearman_string_take_TEST },
  { "new gearman_vector_st() gearman_string_take_string()", 0, gearman_string_allocate_take_TEST },
  { 0, 0, 0 }
};

test_st printf_TESTS[] ={
  { "gearman_vector_st::vec_printf()", 0, gearman_vector_st__vec_printf_TEST },
  { "gearman_vector_st::vec_printf(EMPTY)", 0, gearman_vector_st__vec_printf_empty_TEST },
  { "gearman_vector_st::vec_printf(, NULL)", 0, gearman_vector_st__vec_printf_NULL_TEST },
  { "gearman_vector_st::vec_printf(, string, string)", 0, gearman_vector_st__vec_printf_string2_TEST },
  { 0, 0, 0 }
};

test_st append_printf_TESTS[] ={
  { "gearman_vector_st::vec_append_printf(1)", 0, gearman_vector_st__vec_append_printf_1_TEST },
  { "gearman_vector_st::vec_append_printf()", 0, gearman_vector_st__vec_append_printf_TEST },
  { "gearman_vector_st::vec_append_printf(EMPTY)", 0, gearman_vector_st__vec_append_printf_empty_TEST },
  { "gearman_vector_st::vec_append_printf(, NULL)", 0, gearman_vector_st__vec_append_printf_NULL_TEST },
  { "gearman_vector_st::vec_append_printf(, string, string)", 0, gearman_vector_st__vec_append_printf_string2_TEST },
  { 0, 0, 0 }
};

collection_st collection[] ={
  {"allocate", NULL, NULL, allocate_TESTS },
  {"resize", NULL, NULL, resize_TESTS },
  {"append", NULL, NULL, append_TESTS },
  {"take", NULL, NULL, take_TESTS },
  {"printf", NULL, NULL, printf_TESTS },
  {"append_printf", NULL, NULL, append_printf_TESTS },
  {0, 0, 0, 0}
};

void get_world(libtest::Framework *world)
{
  world->collections(collection);
}
