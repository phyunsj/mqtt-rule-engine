#include <lua.h>
#include <lauxlib.h>
#include <lualib.h> 

#include <stdlib.h>                             
#include <stdio.h>                              
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <sqlite3.h>
#include <assert.h>

#include "filter.lua.h" // test luaL_loadbuffer()

/* http://www.jera.com/techinfo/jtns/jtn002.html */
/* file: minunit.h */
int tests_run;
#define mu_assert(message, test) do { if (!(test)) return message;} while(0)
#define mu_run_test(test) do { char *message = test(); tests_run++; \
                                if (message) return message; } while (0)


const int display_code = 1;                         
#define TEST_BEGIN  printf("%3d : \"%s\" expected ",  tests_run + 1, __FUNCTION__);
#define DISPLAY_CODE(CodeBlock) if ( display_code ) printf("'''%s'''\n", CodeBlock);
/* 
 * LUA error message
 */
void bail(lua_State *L, char *msg){
	fprintf(stderr, "\nFATAL ERROR:\n  %s: %s\n\n", msg, lua_tostring(L, -1));
	exit(-1);
}

/* 
 * sqlite error message
 * Print the most recent database error for database db to standard error.
 */
static void databaseError(sqlite3* db){
  int errcode = sqlite3_errcode(db);
  const char *errmsg = sqlite3_errmsg(db);
  fprintf(stderr, "Database error %d: %s\n", errcode, errmsg);
}

/*
 * sqlite Helper Routines
 * NOTICE : sqlite-related calls were from https://www.sqlite.org/cvstrac/wiki?p=BlobExample 
 */

/*
** Create the code table in database db. Return an SQLite error code.
*/ 
static int create_fTable(sqlite3 *db){
  const char *zSql = "CREATE TABLE funcTable(fName TEXT PRIMARY KEY, fCode BLOB)";
  return sqlite3_exec(db, zSql, 0, 0, 0);
}

/*
** Create the topic table in database db. Return an SQLite error code.
*/ 

static int create_tTable(sqlite3 *db){
  const char *zSql = "CREATE TABLE topicTable(topic TEXT PRIMARY KEY, fName TEXT NOT NULL, fCode BLOB)";
  return sqlite3_exec(db, zSql, 0, 0, 0);
}

/*
** Store code in database db. Return an SQLite error code.
*/ 
static int write_fTable(
  sqlite3 *db,                   /* Database to insert data into */
  const char *zKey,              /* Null-terminated key string */
  const unsigned char *zBlob,    /* Pointer to blob of data */
  int nBlob                      /* Length of data pointed to by zBlob */
){
  const char *zSql = "INSERT INTO funcTable(fName, fCode) VALUES(?, ?)";
  sqlite3_stmt *pStmt;
  int rc;

  do {
    /* Compile the INSERT statement into a virtual machine. */
    rc = sqlite3_prepare(db, zSql, -1, &pStmt, 0);
    if( rc!=SQLITE_OK ){
      return rc;
    }

    /* Bind the key and value data for the new table entry to SQL variables
    ** (the ? characters in the sql statement) in the compiled INSERT 
    ** statement. 
    **
    ** NOTE: variables are numbered from left to right from 1 upwards.
    ** Passing 0 as the second parameter of an sqlite3_bind_XXX() function 
    ** is an error.
    */
    sqlite3_bind_text(pStmt, 1, zKey, -1, SQLITE_STATIC);
    sqlite3_bind_blob(pStmt, 2, zBlob, nBlob, SQLITE_STATIC);

    /* Call sqlite3_step() to run the virtual machine. Since the SQL being
    ** executed is not a SELECT statement, we assume no data will be returned.
    */
    rc = sqlite3_step(pStmt);
    assert( rc!=SQLITE_ROW );

    /* Finalize the virtual machine. This releases all memory and other
    ** resources allocated by the sqlite3_prepare() call above.
    */
    rc = sqlite3_finalize(pStmt);

    /* If sqlite3_finalize() returned SQLITE_SCHEMA, then try to execute
    ** the statement again.
    */
  } while( rc==SQLITE_SCHEMA );

  return rc;
}

/*
** Store topic in database db. Return an SQLite error code.
*/ 
static int write_tTable(
  sqlite3 *db,                   /* Database to insert data into */
  const char *zKey,              /* Null-terminated key string */
  const char *zFunc              /* Null-terminated function name string */
){
  const char *zSql = "INSERT INTO topicTable(topic, fName) VALUES(?, ?)";
  sqlite3_stmt *pStmt;
  int rc;

  do {
    rc = sqlite3_prepare(db, zSql, -1, &pStmt, 0);
    if( rc!=SQLITE_OK ){
      return rc;
    }

    sqlite3_bind_text(pStmt, 1, zKey, -1, SQLITE_STATIC);
    sqlite3_bind_text(pStmt, 2, zFunc, -1, SQLITE_STATIC);

    /* Call sqlite3_step() to run the virtual machine. Since the SQL being
    ** executed is not a SELECT statement, we assume no data will be returned.
    */
    rc = sqlite3_step(pStmt);
    assert( rc!=SQLITE_ROW );

    /* Finalize the virtual machine. This releases all memory and other
    ** resources allocated by the sqlite3_prepare() call above.
    */
    rc = sqlite3_finalize(pStmt);

    /* If sqlite3_finalize() returned SQLITE_SCHEMA, then try to execute
    ** the statement again.
    */
  } while( rc==SQLITE_SCHEMA );

  return rc;
}


/*
** Read a code from database db. Return an SQLite error code.
*/ 
static int read_fTable(
  sqlite3 *db,               /* Database containing blobs table */
  const char *zKey, /* topic */  /* Null-terminated key to retrieve blob for */
  unsigned char **pzBlob,    /* Set *pzBlob to point to the retrieved blob */
  int *pnBlob                /* Set *pnBlob to the size of the retrieved blob */
){

  const char *zSql1 = "SELECT fName FROM topicTable WHERE topic = ?";
  const char *zSql2 = "SELECT fCode FROM funcTable WHERE fName = ?";

  sqlite3_stmt *pStmt;
  int rc;

  /* In case there is no table entry for key zKey or an error occurs, 
  ** set *pzBlob and *pnBlob to 0 now.
  */
  unsigned char *zFunc = 0; 
  int znFunc = 0;
  *pzBlob = 0;
  *pnBlob = 0;

  do {
    /* Compile the SELECT statement into a virtual machine. */
    rc = sqlite3_prepare(db, zSql1, -1, &pStmt, 0);
    if( rc!=SQLITE_OK ){
      databaseError(db);
      return rc;
    }

    /* Bind the key to the SQL variable. */
    sqlite3_bind_text(pStmt, 1, zKey, -1, SQLITE_STATIC);

    /* Run the virtual machine. We can tell by the SQL statement that
    ** at most 1 row will be returned. So call sqlite3_step() once
    ** only. Normally, we would keep calling sqlite3_step until it
    ** returned something other than SQLITE_ROW.
    */
    rc = sqlite3_step(pStmt);
    if( rc==SQLITE_ROW ){
      /* The pointer returned by sqlite3_column_blob() points to memory
      ** that is owned by the statement handle (pStmt). It is only good
      ** until the next call to an sqlite3_XXX() function (e.g. the 
      ** sqlite3_finalize() below) that involves the statement handle. 
      ** So we need to make a copy of the blob into memory obtained from 
      ** malloc() to return to the caller.
      */
      znFunc = sqlite3_column_bytes(pStmt, 0);
      zFunc = (unsigned char *)malloc(znFunc+1);
      memset( zFunc, 0 , znFunc+1);
      memcpy(zFunc, sqlite3_column_text(pStmt, 0), znFunc);
    }

    /* Finalize the statement (this releases resources allocated by 
    ** sqlite3_prepare() ).
    */
    rc = sqlite3_finalize(pStmt);

    /* If sqlite3_finalize() returned SQLITE_SCHEMA, then try to execute
    ** the statement all over again.
    */
  } while( rc==SQLITE_SCHEMA );

  if ( zFunc ) {
  if ( display_code ) printf(" <<< \"%s\"[%d] found >>>  ", zFunc, znFunc);

  do {
    /* Compile the SELECT statement into a virtual machine. */
    rc = sqlite3_prepare(db, zSql2, -1, &pStmt, 0);
    if( rc!=SQLITE_OK ){
      databaseError(db);
      return rc;
    }

    /* Bind the key to the SQL variable. */
    sqlite3_bind_text(pStmt, 1, zFunc, -1, SQLITE_STATIC);

    /* Run the virtual machine. We can tell by the SQL statement that
    ** at most 1 row will be returned. So call sqlite3_step() once
    ** only. Normally, we would keep calling sqlite3_step until it
    ** returned something other than SQLITE_ROW.
    */
    rc = sqlite3_step(pStmt);
    if( rc==SQLITE_ROW ){
      /* The pointer returned by sqlite3_column_blob() points to memory
      ** that is owned by the statement handle (pStmt). It is only good
      ** until the next call to an sqlite3_XXX() function (e.g. the 
      ** sqlite3_finalize() below) that involves the statement handle. 
      ** So we need to make a copy of the blob into memory obtained from 
      ** malloc() to return to the caller.
      */
      *pnBlob = sqlite3_column_bytes(pStmt, 0);
      *pzBlob = (unsigned char *)malloc(*pnBlob +1);
      memset( *pzBlob, 0 , *pnBlob +1);
      memcpy(*pzBlob, sqlite3_column_blob(pStmt, 0), *pnBlob);
    }

    /* Finalize the statement (this releases resources allocated by 
    ** sqlite3_prepare() ).
    */
    rc = sqlite3_finalize(pStmt);

    /* If sqlite3_finalize() returned SQLITE_SCHEMA, then try to execute
    ** the statement all over again.
    */
  } while( rc==SQLITE_SCHEMA );

  }

  if( zFunc ) free(zFunc);
  return rc;
}

/*
** Free a blob read by readBlob().
*/
static void free_fTable(unsigned char *zBlob){
  free(zBlob);
}


/*******************************************
 * Test Case
 ******************************************/

static char* test_do_nothing(void)
{
    TEST_BEGIN

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadfile(L, "nothing.lua"))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushstring( L, "234");
    
    if (lua_pcall(L, 1, 2, 0))
       bail(L, "lua_pcall() failed");

    int returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */

    mu_assert("=> error, returnCode != 1", returnCode == 1);
    mu_assert("=> error, returnMsg != \"234\"", !(strcmp(returnMsg, "234")) );
    printf("nothing : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));
    lua_close(L);                               /* Clean up, free the Lua state var */
    return 0;
}

static char* test_convert_input_as_string(void)
{
    TEST_BEGIN

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadfile(L, "convert.lua"))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushstring( L, "200");
    
    if (lua_pcall(L, 1, 2, 0))
       bail(L, "lua_pcall() failed");

    int returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */

    mu_assert("=> error, returnCode != 1", returnCode == 1);
    mu_assert("=> error, returnMsg != { 'humidity' : 9 }", !(strcmp(returnMsg, "{ 'humidity' : 9 }")) );
    printf("convert : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));
    lua_close(L);                               /* Clean up, free the Lua state var */
    return 0;
}
 
static char* test_convert_input_as_number(void)
{
    TEST_BEGIN

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadfile(L, "convert.lua"))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushnumber( L, 200);
    
    if (lua_pcall(L, 1, 2, 0))
       bail(L, "lua_pcall() failed");

    int returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */   
    printf("convert : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));

    mu_assert("=> error, returnCode != 1", returnCode == 1);
    mu_assert("=> error, returnMsg != { 'humidity' : 9 }", !(strcmp(returnMsg, "{ 'humidity' : 9 }")) );
    lua_close(L);                               /* Clean up, free the Lua state var */
    return 0;
}

static char* test_convert_input_as_non_number(void)
{
    TEST_BEGIN

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadfile(L, "convert.lua"))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushstring( L, "hello");
    
    if (lua_pcall(L, 1, 2, 0))
       bail(L, "lua_pcall() failed");

    int returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */
    printf("convert : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));
    
    mu_assert("=> error, returnCode != 0", returnCode == 0);
    mu_assert("=> error, returnMsg != { 'humidity' : 0 }", !(strcmp(returnMsg, "{ 'humidity' : 0 }")) );
    lua_close(L);                               /* Clean up, free the Lua state var */
    return 0;
}

static char* test_filter_input_as_string(void)
{
    TEST_BEGIN

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadfile(L, "filter.lua"))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushstring( L, "90");
    
    if (lua_pcall(L, 1, 2, 0))
       bail(L, "lua_pcall() failed");

    int returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */
    printf("filter : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));
    
    mu_assert("=> error, returnCode != 0", returnCode == 0);
    mu_assert("=> error, returnMsg != { 'temperature' : 90 }", !(strcmp(returnMsg, "{ 'temperature' : 90 }")) );
    lua_close(L);                               /* Clean up, free the Lua state var */
    return 0;
}

static char* test_filter_input_as_number(void)
{
    TEST_BEGIN

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadfile(L, "filter.lua"))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushnumber( L, 80);
    
    if (lua_pcall(L, 1, 2, 0))
       bail(L, "lua_pcall() failed");

    int returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */
    printf("filter : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));
    
    mu_assert("=> error, returnCode != 0", returnCode == 0);
    mu_assert("=> error, returnMsg != { 'temperature' : 80 }", !(strcmp(returnMsg, "{ 'temperature' : 80 }")) );
    lua_close(L);                               /* Clean up, free the Lua state var */
    return 0;
}

static char* test_filter_input_as_non_number(void)
{
    TEST_BEGIN

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadfile(L, "filter.lua"))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushstring( L, "hello");
    
    if (lua_pcall(L, 1, 2, 0))
       bail(L, "lua_pcall() failed");

    int returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */
    printf("filter : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));
    
    mu_assert("=> error, returnCode != 0", returnCode == 0);
    mu_assert("=> error, returnMsg != { 'temperature' : 0 }", !(strcmp(returnMsg, "{ 'temperature' : 0 }")) );
    lua_close(L);                               /* Clean up, free the Lua state var */
    return 0;
}

static char* test_filter_input_trigger_true(void)
{
    TEST_BEGIN

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadfile(L, "filter.lua"))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushnumber( L, 120);
    
    if (lua_pcall(L, 1, 2, 0))
       bail(L, "lua_pcall() failed");

    int returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */
    printf("filter : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));
    
    mu_assert("=> error, returnCode != 1", returnCode == 1);
    mu_assert("=> error, returnMsg != { 'temperature' : 120 }", !(strcmp(returnMsg, "{ 'temperature' : 120 }")) );
    lua_close(L);                               /* Clean up, free the Lua state var */
    return 0;
}

static char* test_filter_compile_error(void)
{
    TEST_BEGIN

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadfile(L, "compile_error.lua"))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushnumber( L, 120);
    
    if (lua_pcall(L, 1, 2, 0)) {
       mu_assert("=> error, lua_pcall != 0", 1 == 1);
       // bail(L, "lua_pcall() failed"); // Already know that this code won't work
    }
    int returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */
    printf("filter : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));
    
    lua_close(L);                               /* Clean up, free the Lua state var */
    return 0;
}

static char* test_compiled_filter(void)
{
    TEST_BEGIN

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadbuffer(L, (const char *)filter_lua_out , filter_lua_out_len ,"rule.lua"))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushnumber( L, 130);
    
    if (lua_pcall(L, 1, 2, 0)) {
       bail(L, "lua_pcall() failed"); 
    }
    int returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */
    printf("filter : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));
    
    mu_assert("=> error, returnCode != 1", returnCode == 1);
    mu_assert("=> error, returnMsg != { 'temperature' : 130 }", !(strcmp(returnMsg, "{ 'temperature' : 130 }")) );
    lua_close(L);                               /* Clean up, free the Lua state var */
    return 0;
}

static char* create_database(void)
{
    TEST_BEGIN
    
    sqlite3 *db;                  /* Database connection */
    int returnCode;
    
    /* Open the database */
    sqlite3_open("code.db", &db);
    returnCode = sqlite3_errcode(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> open error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    /* Create the blobs table if it has not already been created. We ignore
    ** the error code returned by this call. An error probably just means
    ** the blobs table has already been created anyway.
    */
    returnCode = create_fTable(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> create fTable error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);
    returnCode = create_tTable(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> create tTable error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);
    printf("SQLITE_OK\n");
    return 0;
}

static char* insert_func_noaction_entry(void)
{
    TEST_BEGIN
    
    sqlite3 *db;                  /* Database connection */
    int returnCode;

    /* Open the database */
    sqlite3_open("code.db", &db);
    returnCode = sqlite3_errcode(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> open error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    int fd;
    struct stat sStat;
    int nBlob;
    unsigned char *zBlob;

    /* Open the file. */
    fd = open("nothing.lua", O_RDONLY);
    if( fd < 0 )
      mu_assert("=> file open error, fd < 0", fd > 0);
    
    /* Find out the file size. */
    if( 0 != fstat(fd, &sStat) )
      mu_assert("=> fstate open error, fstate() != 0", 1 == 0);
    
    nBlob = sStat.st_size;

    /* Read the file data into zBlob. Close the file. */
    zBlob = (unsigned char *)malloc(nBlob);
    if( nBlob != read(fd, zBlob, nBlob) )
      mu_assert("=> read error, nBlob != read(fd, zBlob, nBlob) ", nBlob != nBlob);
    close(fd);

    /* Write the blob to the database and free the memory allocated above. */
    returnCode = write_fTable(db, "noaction", zBlob, nBlob);
    if( returnCode !=  SQLITE_OK) databaseError(db);
    mu_assert("=> write fTable error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);
    free(zBlob);   
    printf("SQLITE_OK\n"); 
    return 0;
}

static char* insert_func_filter_entry(void)
{
    TEST_BEGIN
    
    sqlite3 *db;                  /* Database connection */
    int returnCode;

    /* Open the database */
    sqlite3_open("code.db", &db);
    returnCode = sqlite3_errcode(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> open error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    int fd;
    struct stat sStat;
    int nBlob;
    unsigned char *zBlob;

    /* Open the file. */
    fd = open("filter.lua", O_RDONLY);
    if( fd < 0 )
      mu_assert("=> file open error, fd < 0", fd > 0);
   
    /* Find out the file size. */
    if( 0 != fstat(fd, &sStat) )
      mu_assert("=> fstate open error, fstate() != 0", 1 == 0);
  
    nBlob = sStat.st_size;

    /* Read the file data into zBlob. Close the file. */
    zBlob = (unsigned char *)malloc(nBlob);
    if( nBlob != read(fd, zBlob, nBlob) )
      mu_assert("=> read error, nBlob != read(fd, zBlob, nBlob) ", nBlob != nBlob);
    close(fd);

    /* Write the blob to the database and free the memory allocated above. */
    returnCode = write_fTable(db, "filter", zBlob, nBlob);
    if( returnCode !=  SQLITE_OK) databaseError(db);
    mu_assert("=> write fTable error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);
    free(zBlob);   
    printf("SQLITE_OK\n"); 
    return 0;
}

static char* insert_func_convert_entry(void)
{
    TEST_BEGIN
    
    sqlite3 *db;                  /* Database connection */
    int returnCode;

    /* Open the database */
    sqlite3_open("code.db", &db);
    returnCode = sqlite3_errcode(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> open error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    int fd;
    struct stat sStat;
    int nBlob;
    unsigned char *zBlob;

    /* Open the file. */
    fd = open("convert.lua", O_RDONLY);
    if( fd < 0 )
      mu_assert("=> file open error, fd < 0", fd > 0);
   
    /* Find out the file size. */
    if( 0 != fstat(fd, &sStat) )
      mu_assert("=> fstate open error, fstate() != 0", 1 == 0);

    nBlob = sStat.st_size;

    /* Read the file data into zBlob. Close the file. */
    zBlob = (unsigned char *)malloc(nBlob);
    if( nBlob != read(fd, zBlob, nBlob) )
      mu_assert("=> read error, nBlob != read(fd, zBlob, nBlob) ", nBlob != nBlob);
    close(fd);

    /* Write the blob to the database and free the memory allocated above. */
    returnCode = write_fTable(db, "convert", zBlob, nBlob);
    if( returnCode !=  SQLITE_OK) databaseError(db);
    mu_assert("=> write fTable error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);
    free(zBlob);   
    printf("SQLITE_OK\n"); 
    return 0;
}

static char* insert_topic_noaction_entry(void)
{
    TEST_BEGIN
    
    sqlite3 *db;                  /* Database connection */
    int returnCode;

    /* Open the database */
    sqlite3_open("code.db", &db);
    returnCode = sqlite3_errcode(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> open error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    /* Write the blob to the database and free the memory allocated above. */
    returnCode = write_tTable(db, "city/building11/floor1/temperature", "noaction");
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> write tTable error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);
   
    printf("SQLITE_OK\n");
    return 0;
}

static char* insert_topic_filter_entry(void)
{
    TEST_BEGIN
    
    sqlite3 *db;                  /* Database connection */
    int returnCode;

    /* Open the database */
    sqlite3_open("code.db", &db);
    returnCode = sqlite3_errcode(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> open error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    /* Write the blob to the database and free the memory allocated above. */
    returnCode = write_tTable(db, "city/building12/floor1/temperature", "filter");
    if ( returnCode != SQLITE_OK )  databaseError(db);
    mu_assert("=> write tTable error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);
    printf("SQLITE_OK\n");
    return 0;
}

static char* insert_topic_convert_entry(void)
{
    TEST_BEGIN
    
    sqlite3 *db;                  /* Database connection */
    int returnCode;

    /* Open the database */
    sqlite3_open("code.db", &db);
    returnCode = sqlite3_errcode(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> open error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    /* Write the blob to the database and free the memory allocated above. */
    returnCode = write_tTable(db, "city/building12/floor2/humidity", "convert");
    if ( returnCode != SQLITE_OK )  databaseError(db);
    mu_assert("=> write tTable error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);
    printf("SQLITE_OK\n");
    return 0;
}

static char* execute_non_topic_noaction(void)
{
    TEST_BEGIN
    
    sqlite3 *db;                  /* Database connection */
    int nBlob;
    unsigned char *zBlob = 0;
    int returnCode;

    /* Open the database */
    sqlite3_open("code.db", &db);
    returnCode = sqlite3_errcode(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> open error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    returnCode =read_fTable(db, "city/building11/floor14/temperature", &zBlob, &nBlob);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> read_funcTable, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    if( !zBlob )
      printf("topic \"city/building11/floor1/temperature\" not found\n");

    mu_assert("=> error, zBlob != 0", zBlob == 0);

    if( zBlob ) free_fTable(zBlob);
    return 0;    
}

static char* execute_topic_noaction(void)
{
    TEST_BEGIN
    
    sqlite3 *db;                  /* Database connection */
    int nBlob;
    unsigned char *zBlob;
    int returnCode;

    /* Open the database */
    sqlite3_open("code.db", &db);
    returnCode = sqlite3_errcode(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> open error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);


    returnCode = read_fTable(db, "city/building11/floor1/temperature", &zBlob, &nBlob);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> read_funcTable, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    if( !zBlob ){
      printf("No such database entry for ");
      printf("topic \"city/building11/floor1/temperature\" found\n");
    } 

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadstring(L, zBlob))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushstring( L, "120");
    
    if (lua_pcall(L, 1, 2, 0))
       bail(L, "lua_pcall() failed");

    returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */
    printf("noaction : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));
    
    mu_assert("=> error, returnCode != 1", returnCode == 1);
    mu_assert("=> error, returnMsg != \"120\"", !(strcmp(returnMsg, "120")) );
    lua_close(L);                               /* Clean up, free the Lua state var */
    
    /* Free the blob read from the database */
    DISPLAY_CODE(zBlob);
    free_fTable(zBlob);
    return 0;    
}

static char* execute_topic_filter(void)
{
    TEST_BEGIN
    
    sqlite3 *db;                  /* Database connection */
    int nBlob;
    unsigned char *zBlob;
    int returnCode;

    /* Open the database */
    sqlite3_open("code.db", &db);
    returnCode = sqlite3_errcode(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> open error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    returnCode = read_fTable(db, "city/building12/floor1/temperature", &zBlob, &nBlob);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> read_funcTable, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    if( !zBlob ){
      printf("No such database entry for ");
      printf("topic \"city/building12/floor1/temperature\" found\n");
    } 

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadstring(L, zBlob))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushstring( L, "90");
    
    if (lua_pcall(L, 1, 2, 0))
       bail(L, "lua_pcall() failed");

    returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */
    printf("filter : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));
    
    mu_assert("=> error, returnCode != 0", returnCode == 0);
    mu_assert("=> error, returnMsg != { 'temperature' : 90 }", !(strcmp(returnMsg, "{ 'temperature' : 90 }")) );
    lua_close(L);                               /* Clean up, free the Lua state var */
    
    /* Free the blob read from the database */
    DISPLAY_CODE(zBlob);
    free_fTable(zBlob);
    return 0;    
}

static char* execute_topic_convert(void)
{
    TEST_BEGIN
    
    sqlite3 *db;                  /* Database connection */
    int nBlob;
    unsigned char *zBlob;
    int returnCode;

    /* Open the database */
    sqlite3_open("code.db", &db);
    returnCode = sqlite3_errcode(db);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> open error, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    returnCode = read_fTable(db, "city/building12/floor2/humidity", &zBlob, &nBlob);
    if ( returnCode != SQLITE_OK ) databaseError(db);
    mu_assert("=> read_funcTable, returnCode != SQLITE_OK", returnCode == SQLITE_OK);

    if( !zBlob ){
      printf("No such database entry for ");
      printf("topic \"city/building12/floor2/humidity\" found\n");
    } 

    lua_State *L;

    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadstring(L, zBlob))
	    bail(L, "luaL_loadfile() failed");        /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed"); /* Error out if Lua file has an error */

    lua_getglobal(L, "action");

    lua_pushstring( L, "200");
    
    if (lua_pcall(L, 1, 2, 0))
       bail(L, "lua_pcall() failed");

    returnCode = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
    char* returnMsg = strdup(lua_tostring(L, -1)); /* 2nd return parameter : JSON string */
    printf("filter : %d, \"%s\"[%lu]\n", returnCode, returnMsg, strlen(returnMsg));
    
    mu_assert("=> error, returnCode != 1", returnCode == 1);
    mu_assert("=> error, returnMsg != { 'humidity' : 9 }", !(strcmp(returnMsg, "{ 'humidity' : 9 }")) );
    lua_close(L);                               /* Clean up, free the Lua state var */
    
    /* Free the blob read from the database */
    DISPLAY_CODE(zBlob);
    free_fTable(zBlob);
    return 0;    
}

static char * all_tests() {
     mu_run_test(test_do_nothing);
     mu_run_test(test_convert_input_as_string);
     mu_run_test(test_convert_input_as_number);
     mu_run_test(test_convert_input_as_non_number);
     mu_run_test(test_filter_input_as_string);
     mu_run_test(test_filter_input_as_number);
     mu_run_test(test_filter_input_as_non_number);
     mu_run_test(test_filter_input_trigger_true);
     mu_run_test(test_filter_compile_error); 
     mu_run_test(test_compiled_filter);
     mu_run_test(create_database);
     mu_run_test(insert_func_noaction_entry);
     mu_run_test(insert_func_filter_entry);
     mu_run_test(insert_func_convert_entry);
     mu_run_test(insert_topic_noaction_entry);
     mu_run_test(insert_topic_filter_entry);
     mu_run_test(insert_topic_convert_entry);
     mu_run_test(execute_non_topic_noaction);
     mu_run_test(execute_topic_noaction);
     mu_run_test(execute_topic_filter);
     mu_run_test(execute_topic_convert);
     return 0;
 }
 
 int main(int argc, char **argv) {
     char *result = all_tests();
     if (result != 0) {
         printf("%s\n", result);
     }
     else {
         printf("ALL TESTS PASSED\n");
     }
     printf("Tests run: %d\n", tests_run);
 
     return result != 0;
 }