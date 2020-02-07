/*
* Copyright 2018-2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#ifndef GRAPH_ENTITY_H_
#define GRAPH_ENTITY_H_

#include "../../value.h"
#include "../../../deps/GraphBLAS/Include/GraphBLAS.h"

#define ATTRIBUTE_NOTFOUND USHRT_MAX

#define ENTITY_ID_ISLT(a, b) ((*a) < (*b))
#define INVALID_ENTITY_ID -1l

#define ENTITY_GET_ID(graphEntity) ((graphEntity)->entity ? (graphEntity)->entity->id : INVALID_ENTITY_ID)
#define ENTITY_PROP_COUNT(graphEntity) ((graphEntity)->entity->prop_count)
#define ENTITY_TYPE(graphEntity) ((graphEntity)->entity->entity_type)

// Defined in graph_entity.c
extern SIValue *PROPERTY_NOTFOUND;

typedef unsigned short Attribute_ID;
typedef GrB_Index EntityID;
typedef GrB_Index NodeID;
typedef GrB_Index EdgeID;

/*  Format a graph entity string according to the enum.
    One can sum the enum values in order to print multiple value:
    ENTITY_ID + ENTITY_LABELS_OR_RELATIONS will print both id and label. */

typedef enum {
	ENTITY_ID = 1,                       // print id only
	ENTITY_LABELS_OR_RELATIONS = 1 << 1, // print label or relationship type
	ENTITY_PROPERTIES = 1 << 2           // print properties
} GraphEntityStringFromat;

typedef enum GraphEntityType {
	GETYPE_UNKNOWN = 0,
	GETYPE_NODE = 1,
	GETYPE_EDGE = 2
} GraphEntityType;

// Essence of a graph entity.
// TODO: see if pragma pack 0 will cause memory access violation on ARM.
typedef struct {
	EntityID id;                // Unique id
	uint entity_type : 2;       // 1: node, 2: edge.
	int prop_count;             // Number of properties.
} Entity;

// Common denominator between nodes and edges.
typedef struct {
	Entity *entity;
} GraphEntity;

/* Adds property to entity
 * returns - reference to newly added property. */
void GraphEntity_AddProperty(GraphEntity *e, Attribute_ID attr_id, SIValue value);

/* Retrieves entity's property
 * NOTE: If the key does not exist, we set `v` to SI_NullVal. */
void GraphEntity_GetProperty(const GraphEntity *e, Attribute_ID attr_id, SIValue *v);

/* Retrieves all properties assigned to entity
 * attr_id - optional array of length prop_count.
 * attr_name - optional array of length prop_count.
 * v - array of length prop_count. */
void GraphEntity_GetProperties(const GraphEntity *e, Attribute_ID *attr_ids,
							   const char **attr_names, SIValue *vs);

/* Updates existing attribute value. */
void GraphEntity_SetProperty(GraphEntity *e, Attribute_ID attr_id, SIValue value);

/* Prints the graph entity into a buffer, returns what is the string length, buffer can be re-allocated at need. */
void GraphEntity_ToString(const GraphEntity *e, char **buffer, size_t *bufferLen,
						  size_t *bytesWritten, GraphEntityStringFromat format);

/* Release all memory allocated by entity */
void FreeEntity(Entity *e);

#endif

