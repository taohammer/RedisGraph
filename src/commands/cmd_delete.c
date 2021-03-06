/*
* Copyright 2018-2020 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#include "cmd_delete.h"

#include <assert.h>
#include "./cmd_context.h"
#include "../graph/graph.h"
#include "../graph/graphcontext.h"
#include "../query_ctx.h"
#include "../resultset/resultset.h"

extern RedisModuleType *GraphContextRedisModuleType;

/* Delete graph, removing the key from Redis and
 * freeing every resource allocated by the graph. */
int MGraph_Delete(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
	if(argc != 2) return RedisModule_WrongArity(ctx);

	char *strElapsed = NULL;
	QueryCtx_BeginTimer(); // Start deletion timing.

	RedisModuleString *graph_name = argv[1];
	GraphContext *gc = GraphContext_Retrieve(ctx, graph_name, false, false);    // Increase ref count.
	if(!gc) {
		RedisModule_ReplyWithError(ctx, "Graph is either missing or referred key is of a different type.");
		goto cleanup;
	}

	// Remove graph from keyspace.
	RedisModuleKey *key = RedisModule_OpenKey(ctx, graph_name, REDISMODULE_WRITE);
	RedisModule_DeleteKey(key); // Decreases graph ref count.
	GraphContext_Release(gc);  // Decrease graph ref count.

	double t = QueryCtx_GetExecutionTime();
	asprintf(&strElapsed, "Graph removed, internal execution time: %.6f milliseconds", t);
	RedisModule_ReplyWithStringBuffer(ctx, strElapsed, strlen(strElapsed));

cleanup:
	QueryCtx_Free(); // Reset the QueryCtx and free its allocations.
	if(strElapsed) free(strElapsed);
	// Delete commands should always modify slaves.
	RedisModule_ReplicateVerbatim(ctx);
	return REDISMODULE_OK;
}
