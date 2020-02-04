/*
* Copyright 2018-2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#include <stdio.h>
#include <assert.h>
#include "graph_entity.h"
#include "node.h"
#include "edge.h"
#include "../../query_ctx.h"
#include "../graphcontext.h"
#include "../../util/rmalloc.h"
#include "../../GraphBLASExt/GxB_Delete.h"

SIValue *PROPERTY_NOTFOUND = &(SIValue) {
	.longval = 0, .type = T_NULL
};

/* Removes entity's property. */
static void _GraphEntity_RemoveProperty(Entity *e, Attribute_ID attr_id) {
	assert(e);
	if(e->prop_count == 0) return;

	GrB_Matrix m = GrB_NULL;
	Graph *g = QueryCtx_GetGraph();

	if(e->entity_type == GETYPE_NODE) m = Graph_GetNodeAttributeMatrix(g, attr_id);
	else m = Graph_GetEdgeAttributeMatrix(g, attr_id);
	assert(m);

	// Make sure attribute exists.
	SIValue v;
	if(GrB_Matrix_extractElement_UDT(&v, m, e->id, e->id) == GrB_SUCCESS) {
		SIValue_Free(&v);
		assert(GxB_Matrix_Delete(m, e->id, e->id) == GrB_SUCCESS);
		e->prop_count--;
	}
}

/* Add a new property to entity */
void GraphEntity_AddProperty(GraphEntity *e, Attribute_ID attr_id, SIValue value) {
	GrB_Matrix m = GrB_NULL;
	Graph *g = QueryCtx_GetGraph();

	e->entity->prop_count++;
	if(ENTITY_TYPE(e) == GETYPE_NODE) m = Graph_GetNodeAttributeMatrix(g, attr_id);
	else m = Graph_GetEdgeAttributeMatrix(g, attr_id);
	assert(m);

	// TODO: should we test to see we're overwriting existing value?
	SIValue clone = SI_CloneValue(value);
	GrB_Matrix_setElement_UDT(m, &clone, ENTITY_GET_ID(e), ENTITY_GET_ID(e));
}

void GraphEntity_GetProperty(const GraphEntity *e, Attribute_ID attr_id, SIValue *v) {
	if(attr_id == ATTRIBUTE_NOTFOUND || ENTITY_PROP_COUNT(e) == 0) {
		*v = SI_NullVal();
		return;
	}

	GrB_Matrix m = GrB_NULL;
	Graph *g = QueryCtx_GetGraph();

	if(ENTITY_TYPE(e) == GETYPE_NODE) m = Graph_GetNodeAttributeMatrix(g, attr_id);
	else m = Graph_GetEdgeAttributeMatrix(g, attr_id);
	assert(m);

	GrB_Info info = GrB_Matrix_extractElement_UDT(v, m, ENTITY_GET_ID(e), ENTITY_GET_ID(e));
	if(info != GrB_SUCCESS) {
		*v = SI_NullVal();
		return;
	}

	/* As we're dealing with a duplicate of the attribute
	 * (GrB_Matrix_extractElement_UDT performs copy)
	 * make sure receiver of attribute can't free it. */
	if(v->allocation != M_NONE) v->allocation = M_CONST;
}

void GraphEntity_GetProperties(
	const GraphEntity *e,
	Attribute_ID *attr_ids,
	const char **attr_names,
	SIValue *vs
) {
	assert(e && vs);

	SIValue v;
	GrB_Info info;
	GrB_Matrix m = GrB_NULL;
	Attribute_ID attr_id = 0;
	Graph *g = QueryCtx_GetGraph();
	int attr_count = ENTITY_PROP_COUNT(e);
	GraphContext *gc = QueryCtx_GetGraphCtx();

	// As long as there are attributes to be retrieved.
	while(attr_count) {
		if(ENTITY_TYPE(e) == GETYPE_NODE) m = Graph_GetNodeAttributeMatrix(g, attr_id);
		else m = Graph_GetEdgeAttributeMatrix(g, attr_id);
		assert(m);

		// See if entity contains attribute `attr_id`.
		info = GrB_Matrix_extractElement_UDT(&v, m, ENTITY_GET_ID(e), ENTITY_GET_ID(e));
		if(info == GrB_SUCCESS) {
			if(attr_ids) attr_ids[attr_count - 1] = attr_id;
			if(attr_names) attr_names[attr_count - 1] = GraphContext_GetAttributeString(gc, attr_id);

			/* As we're dealing with a duplicate of the attribute
			* (GrB_Matrix_extractElement_UDT performs copy)
			* make sure receiver of attribute can't free it. */
			if(v.allocation != M_NONE) v.allocation = M_CONST;
			vs[attr_count - 1] = v;

			attr_count--;
		}

		attr_id++;
	}
}

// Updates existing property value.
void GraphEntity_SetProperty(GraphEntity *e, Attribute_ID attr_id, SIValue value) {
	assert(e);

	// Start by removing previous value.
	_GraphEntity_RemoveProperty(e->entity, attr_id);

	// Setting an attribute value to NULL removes that attribute.
	if(SIValue_IsNull(value)) return;

	GraphEntity_AddProperty(e, attr_id, SI_CloneValue(value));
}

size_t GraphEntity_PropertiesToString(const GraphEntity *e, char **buffer, size_t *bufferLen,
									  size_t *bytesWritten) {
	// make sure there is enough space for "{...}\0"
	if(*bufferLen - *bytesWritten < 64) {
		*bufferLen += 64;
		*buffer = rm_realloc(*buffer, *bufferLen);
	}
	*bytesWritten += snprintf(*buffer, *bufferLen, "{");
	GraphContext *gc = QueryCtx_GetGraphCtx();
	SIValue v;
	Attribute_ID attr_id = 0;
	int propCount = ENTITY_PROP_COUNT(e);

	while(propCount) {
		GraphEntity_GetProperty(e, attr_id++, &v);
		if(SIValue_IsNull(v)) {
			attr_id++;
			continue;
		}
		propCount--;

		// print key
		const char *key = GraphContext_GetAttributeString(gc, attr_id);
		// check for enough space
		size_t keyLen = strlen(key);
		if(*bufferLen - *bytesWritten < keyLen) {
			*bufferLen += keyLen;
			*buffer = rm_realloc(*buffer, *bufferLen);
		}
		*bytesWritten += snprintf(*buffer + *bytesWritten, *bufferLen, "%s:", key);

		// print value
		SIValue_ToString(v, buffer, bufferLen, bytesWritten);

		// if not the last element print ", "
		if(propCount) *bytesWritten = snprintf(*buffer + *bytesWritten, *bufferLen, ", ");

		// Advance to the next attribute.
		attr_id++;
	}

	// check for enough space for close with "}\0"
	if(*bufferLen - *bytesWritten < 2) {
		*bufferLen += 2;
		*buffer = rm_realloc(*buffer, *bufferLen);
	}
	*bytesWritten += snprintf(*buffer + *bytesWritten, *bufferLen, "}");
	return *bytesWritten;
}

void GraphEntity_ToString(const GraphEntity *e, char **buffer, size_t *bufferLen,
						  size_t *bytesWritten, GraphEntityStringFromat format) {
	// space allocation
	if(*bufferLen - *bytesWritten < 64)  {
		*bufferLen += 64;
		*buffer = rm_realloc(*buffer, sizeof(char) * *bufferLen);
	}

	// get open an close symbols
	char *openSymbole;
	char *closeSymbole;
	if(ENTITY_TYPE(e) == GETYPE_NODE) {
		openSymbole = "(";
		closeSymbole = ")";
	} else {
		openSymbole = "[";
		closeSymbole = "]";
	}
	*bytesWritten += snprintf(*buffer + *bytesWritten, *bufferLen, "%s", openSymbole);

	// write id
	if(format & ENTITY_ID) {
		*bytesWritten += snprintf(*buffer + *bytesWritten, *bufferLen, "%llu", ENTITY_GET_ID(e));
	}

	// write label
	if(format & ENTITY_LABELS_OR_RELATIONS) {
		switch(ENTITY_TYPE(e)) {
		case GETYPE_NODE: {
			Node *n = (Node *)e;
			if(n->label) {
				// allocate space if needed
				size_t labelLen = strlen(n->label);
				if(*bufferLen - *bytesWritten < labelLen) {
					*bufferLen += labelLen;
					*buffer = rm_realloc(*buffer, sizeof(char) * *bufferLen);
				}
				*bytesWritten += snprintf(*buffer + *bytesWritten, *bufferLen, ":%s", n->label);
			}
			break;
		}

		case GETYPE_EDGE: {
			Edge *edge = (Edge *)e;
			if(edge->relationship) {
				size_t relationshipLen = strlen(edge->relationship);
				if(*bufferLen - *bytesWritten < relationshipLen) {
					*bufferLen += relationshipLen;
					*buffer = rm_realloc(*buffer, sizeof(char) * *bufferLen);
				}
				*bytesWritten += snprintf(*buffer + *bytesWritten, *bufferLen, ":%s", edge->relationship);
			}
			break;
		}

		default:
			assert(false);
		}
	}

	// write properies
	if(format & ENTITY_PROPERTIES) {
		GraphEntity_PropertiesToString(e, buffer, bufferLen, bytesWritten);
	}

	// check for enough space for close with closing symbol
	if(*bufferLen - *bytesWritten < 2) {
		*bufferLen += 2;
		*buffer = rm_realloc(*buffer, sizeof(char) * *bufferLen);
	}
	*bytesWritten += snprintf(*buffer + *bytesWritten, *bufferLen, "%s", closeSymbole);
}

void FreeEntity(Entity *e) {
	assert(e);
	Attribute_ID attr_id = 0;
	while(e->prop_count) _GraphEntity_RemoveProperty(e, attr_id++);
}
