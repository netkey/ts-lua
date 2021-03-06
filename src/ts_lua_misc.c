/*
  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0
 
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/


#include "ts_lua_util.h"


static int ts_lua_get_now_time(lua_State *L);
static int ts_lua_debug(lua_State *L);
static int ts_lua_error(lua_State *L);
static int ts_lua_sleep(lua_State *L);

static int ts_lua_sleep_cleanup(ts_lua_async_item *ai);
static int ts_lua_sleep_handler(TSCont contp, TSEvent event, void *edata);

void
ts_lua_inject_misc_api(lua_State *L)
{
    /* ts.now() */
    lua_pushcfunction(L, ts_lua_get_now_time);
    lua_setfield(L, -2, "now");

    /* ts.debug(...) */
    lua_pushcfunction(L, ts_lua_debug);
    lua_setfield(L, -2, "debug");

    /* ts.error(...) */
    lua_pushcfunction(L, ts_lua_error);
    lua_setfield(L, -2, "error");

    /* ts.sleep(...) */
    lua_pushcfunction(L, ts_lua_sleep);
    lua_setfield(L, -2, "sleep");
}

static int
ts_lua_get_now_time(lua_State *L)
{
    time_t    now;

    now = TShrtime() / 1000000000;
    lua_pushnumber(L, now);
    return 1;
}

static int
ts_lua_debug(lua_State *L)
{
    const char      *msg;

    msg = luaL_checkstring(L, 1);
    TSDebug(TS_LUA_DEBUG_TAG, msg, NULL);
    return 0;
}

static int
ts_lua_error(lua_State *L)
{
    const char      *msg;

    msg = luaL_checkstring(L, 1);
    TSError(msg, NULL);
    return 0;
}


static int
ts_lua_sleep(lua_State *L)
{
    int                     sec;
    TSAction                action;
    TSCont                  contp;
    ts_lua_async_item       *ai;
    ts_lua_cont_info        *ci;

    ci = ts_lua_get_cont_info(L);
    if (ci == NULL)
        return 0;

    sec = luaL_checknumber(L, 1);
    if (sec < 1) {
        sec = 1;
    }

    contp = TSContCreate(ts_lua_sleep_handler, ci->mutex);
    action = TSContSchedule(contp, sec * 1000, TS_THREAD_POOL_DEFAULT);

    ai = ts_lua_async_create_item(contp, ts_lua_sleep_cleanup, (void*)action, ci);
    TSContDataSet(contp, ai);

    return lua_yield(L, 0);
}

static int
ts_lua_sleep_handler(TSCont contp, TSEvent event, void *edata)
{
    ts_lua_async_item       *ai;
    ts_lua_cont_info        *ci;

    ai = TSContDataGet(contp);
    ci = ai->cinfo;

    ai->data = NULL;
    ts_lua_sleep_cleanup(ai);

    TSContCall(ci->contp, TS_LUA_EVENT_COROUTINE_CONT, 0);

    return 0;
}

static int
ts_lua_sleep_cleanup(ts_lua_async_item *ai)
{
    if (ai->data) {
        TSActionCancel((TSAction)ai->data);
        ai->data = NULL;
    }

    TSContDestroy(ai->contp);
    ai->deleted = 1;

    return 0;
}
