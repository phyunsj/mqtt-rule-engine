/*
Copyright (c) 2009-2018 Roger Light <roger@atchoo.org>

All rights reserved. This program and the accompanying materials
are made available under the terms of the Eclipse Public License v1.0
and Eclipse Distribution License v1.0 which accompany this distribution.

The Eclipse Public License is available at
   http://www.eclipse.org/legal/epl-v10.html
and the Eclipse Distribution License is available at
  http://www.eclipse.org/org/documents/edl-v10.php.

Contributors:
   Roger Light - initial implementation and documentation.
*/

#include "config.h"

#include <assert.h>
#include <string.h>

#ifdef WITH_BROKER
#  include "mosquitto_broker_internal.h"
#  include "sys_tree.h"
#else
#  define G_PUB_BYTES_SENT_INC(A)
#endif

#include "mosquitto.h"
#include "mosquitto_internal.h"
#include "logging_mosq.h"
#include "mqtt3_protocol.h"
#include "memory_mosq.h"
#include "net_mosq.h"
#include "packet_mosq.h"
#include "send_mosq.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h> 
#include <sqlite3.h>

int send__publish(struct mosquitto *mosq, uint16_t mid, const char *topic, uint32_t payloadlen, const void *payload, int qos, bool retain, bool dup)
{
#ifdef WITH_BROKER
	size_t len;
#ifdef WITH_BRIDGE
	int i;
	struct mosquitto__bridge_topic *cur_topic;
	bool match;
	int rc;
	char *mapped_topic = NULL;
	char *topic_temp = NULL;
#endif
#endif
	assert(mosq);
	assert(topic);

#if defined(WITH_BROKER) && defined(WITH_WEBSOCKETS)
	if(mosq->sock == INVALID_SOCKET && !mosq->wsi) return MOSQ_ERR_NO_CONN;
#else
	if(mosq->sock == INVALID_SOCKET) return MOSQ_ERR_NO_CONN;
#endif

#ifdef WITH_BROKER
	if(mosq->listener && mosq->listener->mount_point){
		len = strlen(mosq->listener->mount_point);
		if(len < strlen(topic)){
			topic += len;
		}else{
			/* Invalid topic string. Should never happen, but silently swallow the message anyway. */
			return MOSQ_ERR_SUCCESS;
		}
	}
#ifdef WITH_BRIDGE
	if(mosq->bridge && mosq->bridge->topics && mosq->bridge->topic_remapping){
		for(i=0; i<mosq->bridge->topic_count; i++){
			cur_topic = &mosq->bridge->topics[i];
			if((cur_topic->direction == bd_both || cur_topic->direction == bd_out)
					&& (cur_topic->remote_prefix || cur_topic->local_prefix)){
				/* Topic mapping required on this topic if the message matches */

				rc = mosquitto_topic_matches_sub(cur_topic->local_topic, topic, &match);
				if(rc){
					return rc;
				}
				if(match){
					mapped_topic = mosquitto__strdup(topic);
					if(!mapped_topic) return MOSQ_ERR_NOMEM;
					if(cur_topic->local_prefix){
						/* This prefix needs removing. */
						if(!strncmp(cur_topic->local_prefix, mapped_topic, strlen(cur_topic->local_prefix))){
							topic_temp = mosquitto__strdup(mapped_topic+strlen(cur_topic->local_prefix));
							mosquitto__free(mapped_topic);
							if(!topic_temp){
								return MOSQ_ERR_NOMEM;
							}
							mapped_topic = topic_temp;
						}
					}

					if(cur_topic->remote_prefix){
						/* This prefix needs adding. */
						len = strlen(mapped_topic) + strlen(cur_topic->remote_prefix)+1;
						topic_temp = mosquitto__malloc(len+1);
						if(!topic_temp){
							mosquitto__free(mapped_topic);
							return MOSQ_ERR_NOMEM;
						}
						snprintf(topic_temp, len, "%s%s", cur_topic->remote_prefix, mapped_topic);
						topic_temp[len] = '\0';
						mosquitto__free(mapped_topic);
						mapped_topic = topic_temp;
					}
					log__printf(NULL, MOSQ_LOG_DEBUG, "Sending PUBLISH to %s (d%d, q%d, r%d, m%d, '%s', ... (%ld bytes))", mosq->id, dup, qos, retain, mid, mapped_topic, (long)payloadlen);
					G_PUB_BYTES_SENT_INC(payloadlen);
					rc =  send__real_publish(mosq, mid, mapped_topic, payloadlen, payload, qos, retain, dup);
					mosquitto__free(mapped_topic);
					return rc;
				}
			}
		}
	}
#endif
	log__printf(NULL, MOSQ_LOG_DEBUG, "Sending PUBLISH to %s (d%d, q%d, r%d, m%d, '%s', ... (%ld bytes))", mosq->id, dup, qos, retain, mid, topic, (long)payloadlen);
	G_PUB_BYTES_SENT_INC(payloadlen);
#else
	log__printf(mosq, MOSQ_LOG_DEBUG, "Client %s sending PUBLISH (d%d, q%d, r%d, m%d, '%s', ... (%ld bytes))", mosq->id, dup, qos, retain, mid, topic, (long)payloadlen);
#endif

	return send__real_publish(mosq, mid, topic, payloadlen, payload, qos, retain, dup);
}

// mosquitto__rule_engine only read code table to execute lua script based on topic
#define bail( L, msg, ret ) \
    { printf("\nFATAL ERROR:\n  %s: %s\n\n", msg, lua_tostring(L, -1)); return ret; }

static void databaseError(sqlite3* db){
  int errcode = sqlite3_errcode(db);
  const char *errmsg = sqlite3_errmsg(db);
  printf("Database error %d: %s\n", errcode, errmsg);
}

/*
** Read a code from database db. Return an SQLite error code.
*/ 
static int read_fTable(
  sqlite3 *db,               /* Database containing blobs table */
  const char *zKey,          /* topic : Null-terminated key to retrieve blob for */
  unsigned char **pzBlob,    /* Set *pzBlob to point to the retrieved blob */
  int *pnBlob                /* Set *pnBlob to the size of the retrieved blob */
){

  const char *zSql1 = "SELECT fName FROM topicTable WHERE topic = ?";
  const char *zSql2 = "SELECT fCode FROM funcTable WHERE fName = ?";

  sqlite3_stmt *pStmt;
  int rc;

  unsigned char *zFunc = 0; 
  int znFunc = 0;
  *pzBlob = 0;
  *pnBlob = 0;

  do {
    rc = sqlite3_prepare(db, zSql1, -1, &pStmt, 0);
    if( rc!=SQLITE_OK ){
      databaseError(db);
      return rc;
    }

    sqlite3_bind_text(pStmt, 1, zKey, -1, SQLITE_STATIC);

    rc = sqlite3_step(pStmt);
    if( rc==SQLITE_ROW ){
      znFunc = sqlite3_column_bytes(pStmt, 0);
      zFunc = (unsigned char *)malloc(znFunc+1);
      memset( zFunc, 0 , znFunc+1);
      memcpy(zFunc, sqlite3_column_text(pStmt, 0), znFunc);
    }

    rc = sqlite3_finalize(pStmt);

  } while( rc==SQLITE_SCHEMA );

  if ( zFunc ) {
     printf(" <<< \"%s\"[%d] found >>>  ", zFunc, znFunc);

  do {
    rc = sqlite3_prepare(db, zSql2, -1, &pStmt, 0);
    if( rc!=SQLITE_OK ){
      databaseError(db);
      return rc;
    }

    sqlite3_bind_text(pStmt, 1, zFunc, -1, SQLITE_STATIC);

    rc = sqlite3_step(pStmt);
    if( rc==SQLITE_ROW ){
      *pnBlob = sqlite3_column_bytes(pStmt, 0);
      *pzBlob = (unsigned char *)malloc(*pnBlob +1);
      memset( *pzBlob, 0 , *pnBlob +1);
      memcpy(*pzBlob, sqlite3_column_blob(pStmt, 0), *pnBlob);
    }

    rc = sqlite3_finalize(pStmt);
  } while( rc==SQLITE_SCHEMA );

  }

  if( zFunc ) free(zFunc);
  return rc;
}

int  mosquitto__rule_engine( const char *topic, const void *payload , char *mqtt_out )
{
	log__printf(NULL, MOSQ_LOG_DEBUG, "[%s] payload \"%s\"\n", __FUNCTION__, payload);
    
	int mqtt_out_flag = 1;
	// DB Access
	sqlite3 *db;   
	int nBlob;
    unsigned char *zBlob;
    int returnCode;

    // if error, just send the original payload
    memcpy(mqtt_out, payload, sizeof(payload));

    /* Open the database */
    /* Idealy db pointer can be initialized when broker is started and maintained in struct mosquitto. Must change mosquitto__rule_engine function prototype in that approach  */
    sqlite3_open("code.db", &db);
    if( SQLITE_OK != sqlite3_errcode(db)) 
       return mqtt_out_flag;
    
    if( SQLITE_OK != read_fTable(db, topic, &zBlob, &nBlob))
       return mqtt_out_flag;

    if( !zBlob ){
      printf("No such database entry for topic \"%s\" found\n", topic);
	  return mqtt_out_flag;;
    } 

    // Execute Rule if applicable

    lua_State *L;
    
    L = luaL_newstate();                         /* Create Lua state variable */
    luaL_openlibs(L);                            /* Load Lua libraries */

    if (luaL_loadstring(L, zBlob)) {
	    bail(L, "luaL_loadfile() failed", 0);        /* Error out if file can't be read */
	} else {

      if (lua_pcall(L, 0, 0, 0))                   /* Run the loaded Lua script/PRIMING RUN */
	    bail(L, "lua_pcall(priming run) failed", 0); /* Error out if Lua file has an error */

      lua_getglobal(L, "action");

	  lua_pushstring(L, payload);

      if (lua_pcall(L, 1, 2, 0)) 
         bail(L, "lua_pcall() failed", 0);

      mqtt_out_flag = lua_tonumber(L, -2);          /* 1st return parameter : true or false */
      strcpy(mqtt_out, lua_tostring(L, -1));        /* 2nd return parameter : JSON string */
      log__printf(NULL, MOSQ_LOG_DEBUG, "[%s] %d, \"%s\":%lu\n", __FUNCTION__, mqtt_out_flag, mqtt_out, strlen(mqtt_out));

      free(zBlob);
    }
    lua_close(L);                               /* Clean up, free the Lua state var */

    return mqtt_out_flag;
}

int send__real_publish(struct mosquitto *mosq, uint16_t mid, const char *topic, uint32_t payloadlen, const void *payload, int qos, bool retain, bool dup)
{
	log__printf(NULL, MOSQ_LOG_DEBUG, "[%s] mid %d, topic %s, payload [%s], len %d, client address %s, client id %s", __FUNCTION__, mid, topic, payload, payloadlen, mosq->address, mosq->id);
	struct mosquitto__packet *packet = NULL;
	char   mosquitto__packet_payload[100]; // limited to 100 chars for the test purpose. Ideally use a heap variable
	int packetlen;
	int rc;

	assert(mosq);
	assert(topic);

    // Apply rules to build "packet"
	if(payloadlen > 0 &&  mosquitto__rule_engine( topic, payload, mosquitto__packet_payload ) ) {
            
		log__printf(NULL, MOSQ_LOG_DEBUG, "[%s] reformat : payload \"%s\":%lu\n", __FUNCTION__, mosquitto__packet_payload, strlen(mosquitto__packet_payload));
		payloadlen = strlen(mosquitto__packet_payload);
		
		packetlen = 2+strlen(topic) + payloadlen;
		if(qos > 0) packetlen += 2; /* For message id */
		packet = mosquitto__calloc(1, sizeof(struct mosquitto__packet));
		if(!packet) return MOSQ_ERR_NOMEM;

		packet->mid = mid;
		packet->command = PUBLISH | ((dup&0x1)<<3) | (qos<<1) | retain;
		packet->remaining_length = packetlen;
		rc = packet__alloc(packet);
		if(rc){
			mosquitto__free(packet);
			return rc;
		}
		/* Variable header (topic string) */
		packet__write_string(packet, topic, strlen(topic));
		if(qos > 0){
			packet__write_uint16(packet, mid);
		}

		/* Payload */
		packet__write_bytes(packet, mosquitto__packet_payload, payloadlen);

	} else return MOSQ_ERR_SUCCESS;

	return packet__queue(mosq, packet);
}
