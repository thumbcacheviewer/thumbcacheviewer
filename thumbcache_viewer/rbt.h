/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2014 Eric Kutcher

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// This Red-Black tree is based on an implementation by Thomas Niemann.
// Refer to "Sorting and Searching Algorithms: A Cookbook"

#ifndef RBT_H
#define RBT_H

typedef enum
{
	RBT_STATUS_OK,
	RBT_STATUS_MEM_EXHAUSTED,
	RBT_STATUS_DUPLICATE_KEY,
	RBT_STATUS_KEY_NOT_FOUND
} rbt_status;

typedef enum
{ 
	BLACK,
	RED
} node_color;

typedef struct node
{
	struct node *left;		// Left child
	struct node *right;		// Right child
	struct node *parent;	// Parent
	node_color color;		// Node color (BLACK, RED)
	void *key;				// Key used for searching
	void *val;				// User data
} node_type;

typedef void rbt_iterator;
typedef void rbt_tree;

// Create a red-black tree and set the comparison function.
rbt_tree *rbt_create( int( *compare )( void *a, void *b ) );

// Insert a key/value pair.
rbt_status rbt_insert( rbt_tree *tree, void *key, void *value );

// Removes a node from the tree. Does not free the key/value pair.
rbt_status rbt_remove( rbt_tree *tree, rbt_iterator *i );

// Returns an iterator or value associated with a key.
rbt_iterator *rbt_find( rbt_tree *tree, void *key, bool return_value );

// Destroy the red-black tree. Does not free the key/value pair.
void rbt_delete( rbt_tree *tree );

#endif
