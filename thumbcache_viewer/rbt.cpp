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

#include <stdlib.h>

#include "rbt.h"

typedef struct tag
{
	node_type *root;	// Root node of red-black tree.
	node_type sentinel;
	int ( *compare )( void *a, void *b );	// Function pointer to comparison function.
} tag_type;

rbt_tree *rbt_create( int( *compare )( void *a, void *b ) )
{
	tag_type *rbt;

	if ( ( rbt = ( tag_type * )malloc( sizeof( tag_type ) ) ) == NULL )
	{
		return NULL;
	}

	rbt->compare = compare;
	rbt->root = &rbt->sentinel;
	rbt->sentinel.left = &rbt->sentinel;
	rbt->sentinel.right = &rbt->sentinel;
	rbt->sentinel.parent = NULL;
	rbt->sentinel.color = BLACK;
	rbt->sentinel.key = NULL;
	rbt->sentinel.val = NULL;

	return rbt;
}

static void delete_tree( rbt_tree *tree, node_type *p )
{
	tag_type *rbt = ( tag_type * )tree;

	// Erase nodes in depth-first traversal.
	if ( p == &rbt->sentinel )
	{
		return;
	}

	delete_tree( tree, p->left );
	delete_tree( tree, p->right );
	free( p );
}

void rbt_delete( rbt_tree *tree )
{
	if ( tree == NULL )
	{
		return;
	}

	tag_type *rbt = ( tag_type * )tree;

	delete_tree( tree, rbt->root );
	free( rbt );
}

static void rotate_left( tag_type *rbt, node_type *x )
{
	// Rotate node x to the left
	node_type *y = x->right;

	// Establish x->right link
	x->right = y->left;
	if ( y->left != &rbt->sentinel )
	{
		y->left->parent = x;
	}

	// Establish y->parent link
	if ( y != &rbt->sentinel )
	{
		y->parent = x->parent;
	}

	if ( x->parent )
	{
		if ( x == x->parent->left )
		{
			x->parent->left = y;
		}
		else
		{
			x->parent->right = y;
		}
	}
	else
	{
		rbt->root = y;
	}

	// Link x and y
	y->left = x;
	if ( x != &rbt->sentinel )
	{
		x->parent = y;
	}
}

static void rotate_right( tag_type *rbt, node_type *x )
{
	// Rotate node x to the right
	node_type *y = x->left;

	// Establish x->left link
	x->left = y->right;
	if ( y->right != &rbt->sentinel )
	{
		y->right->parent = x;
	}

	// Establish y->parent link
	if ( y != &rbt->sentinel )
	{
		y->parent = x->parent;
	}

	if ( x->parent )
	{
		if ( x == x->parent->right )
		{
			x->parent->right = y;
		}
		else
		{
			x->parent->left = y;
		}
	}
	else
	{
		rbt->root = y;
	}

	// Link x and y
	y->right = x;
	if ( x != &rbt->sentinel )
	{
		x->parent = y;
	}
}

static void insert_fixup( tag_type *rbt, node_type *x )
{
	// Maintain red-black tree balance after inserting node x and check red-black properties
	while ( x != rbt->root && x->parent->color == RED )
	{
		// We have a violation
		if ( x->parent == x->parent->parent->left )
		{
			node_type *y = x->parent->parent->right;
			if ( y->color == RED )
			{
				// Uncle is RED
				x->parent->color = BLACK;
				y->color = BLACK;
				x->parent->parent->color = RED;
				x = x->parent->parent;
			}
			else
			{
				// Uncle is BLACK
				if ( x == x->parent->right )
				{
					// Make x a left child
					x = x->parent;
					rotate_left( rbt, x );
				}

				// Recolor and rotate
				x->parent->color = BLACK;
				x->parent->parent->color = RED;
				rotate_right( rbt, x->parent->parent );
			}
		}
		else
		{
			// Mirror image of above code
			node_type *y = x->parent->parent->left;
			if ( y->color == RED )
			{
				// Uncle is RED
				x->parent->color = BLACK;
				y->color = BLACK;
				x->parent->parent->color = RED;
				x = x->parent->parent;
			}
			else
			{
				// Uncle is BLACK
				if ( x == x->parent->left )
				{
					x = x->parent;
					rotate_right( rbt, x );
				}
				x->parent->color = BLACK;
				x->parent->parent->color = RED;
				rotate_left( rbt, x->parent->parent );
			}
		}
	}

	rbt->root->color = BLACK;
}

rbt_status rbt_insert( rbt_tree *tree, void *key, void *val )
{
	if ( tree == NULL )
	{
		return RBT_STATUS_TREE_NOT_FOUND;
	}

	node_type *current, *parent, *x;
	tag_type *rbt = ( tag_type * )tree;

	// Allocate node for data and insert in tree, then find future parent
	current = rbt->root;
	parent = 0;
	while ( current != &rbt->sentinel )
	{
		int rc = rbt->compare( key, current->key );
		if ( rc == 0 )
		{
			return RBT_STATUS_DUPLICATE_KEY;
		}
		parent = current;
		current = ( rc < 0 ) ? current->left : current->right;
	}

	// Setup new node
	if ( ( x = ( node_type * )malloc( sizeof( *x ) ) ) == 0 )
	{
		return RBT_STATUS_MEM_EXHAUSTED;
	}
	x->parent = parent;
	x->left = &rbt->sentinel;
	x->right = &rbt->sentinel;
	x->color = RED;
	x->key = key;
	x->val = val;

	// Insert node in tree
	if ( parent )
	{
		if ( rbt->compare( key, parent->key ) < 0 )
		{
			parent->left = x;
		}
		else
		{
			parent->right = x;
		}
	}
	else
	{
		rbt->root = x;
	}

	insert_fixup( rbt, x );

	return RBT_STATUS_OK;
}

void delete_fixup( tag_type *rbt, node_type *x )
{
	// Maintain red-black tree balance after deleting node x
	while ( x != rbt->root && x->color == BLACK )
	{
		if ( x == x->parent->left )
		{
			node_type *w = x->parent->right;
			if ( w->color == RED )
			{
				w->color = BLACK;
				x->parent->color = RED;
				rotate_left( rbt, x->parent );
				w = x->parent->right;
			}

			if ( w->left->color == BLACK && w->right->color == BLACK )
			{
				w->color = RED;
				x = x->parent;
			}
			else
			{
				if ( w->right->color == BLACK )
				{
					w->left->color = BLACK;
					w->color = RED;
					rotate_right( rbt, w );
					w = x->parent->right;
				}

				w->color = x->parent->color;
				x->parent->color = BLACK;
				w->right->color = BLACK;
				rotate_left( rbt, x->parent );
				x = rbt->root;
			}
		}
		else
		{
			node_type *w = x->parent->left;
			if ( w->color == RED )
			{
				w->color = BLACK;
				x->parent->color = RED;
				rotate_right( rbt, x->parent );
				w = x->parent->left;
			}

			if ( w->right->color == BLACK && w->left->color == BLACK )
			{
				w->color = RED;
				x = x->parent;
			}
			else
			{
				if ( w->left->color == BLACK )
				{
					w->right->color = BLACK;
					w->color = RED;
					rotate_left( rbt, w );
					w = x->parent->left;
				}

				w->color = x->parent->color;
				x->parent->color = BLACK;
				w->left->color = BLACK;
				rotate_right( rbt, x->parent );
				x = rbt->root;
			}
		}
	}

	x->color = BLACK;
}

rbt_status rbt_remove( rbt_tree *tree, rbt_iterator *i )
{
	if ( tree == NULL || i == NULL )
	{
		return RBT_STATUS_KEY_NOT_FOUND;
	}

	node_type *x, *y;
	tag_type *rbt = ( tag_type * )tree;
	node_type *z = ( node_type * )i;

	if ( z->left == &rbt->sentinel || z->right == &rbt->sentinel )
	{
		y = z;	// y has a &rbt->sentinel node as a child
	}
	else
	{
		// Find tree successor with a &rbt->sentinel node as a child
		y = z->right;
		while ( y->left != &rbt->sentinel )
		{
			y = y->left;
		}
	}

	// x is y's only child
	if ( y->left != &rbt->sentinel )
	{
		x = y->left;
	}
	else
	{
		x = y->right;
	}

	// Remove y from the parent chain
	x->parent = y->parent;
	if ( y->parent )
	{
		if ( y == y->parent->left )
		{
			y->parent->left = x;
		}
		else
		{
			y->parent->right = x;
		}
	}
	else
	{
		rbt->root = x;
	}

	if ( y != z )
	{
		z->key = y->key;
		z->val = y->val;
	}

	if ( y->color == BLACK )
	{
		delete_fixup( rbt, x );
	}

	free( y );

	return RBT_STATUS_OK;
}

rbt_iterator *rbt_find( rbt_tree *tree, void *key, bool return_value )
{
	if ( tree == NULL )
	{
		return NULL;
	}

	tag_type *rbt = ( tag_type * )tree;

	node_type *current = rbt->root;
	while ( current != &rbt->sentinel )
	{
		int rc = rbt->compare( key, current->key );
		if ( rc == 0 )
		{
			return ( return_value == true ? current->val : current );
		}
		current = ( rc < 0 ) ? current->left : current->right;
	}

	return NULL;
}
