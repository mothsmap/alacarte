/**
 *  This file is part of alaCarte.
 *
 *  alaCarte is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  alaCarte is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with alaCarte. If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright alaCarte 2012-2013 Simon Dreher, Florian Jacob, Tobias Kahlert, Patrick Niklaus, Bernhard Scheirle, Lisa Winter
 *  Maintainer: Lisa Winter
 */

#include "includes.hpp"
/*
 * =====================================================================================
 *
 *       Filename: nodeKdTree.cpp
 *
 *    Description:  This is the implementation of a 2 demension kd-tree. It functions as
 *    datastructer behind geodata.
 *    As we do not want to delete any content we do not provide a delete function.
 *    Also a tree is only build once and it is not possible to add any new nodes after it has
 *    been constructed.
 *
 * =====================================================================================
 */
#include "general/nodeKdTree.hpp"
#include "general/node.hpp"
#include "utils/rect.hpp"
#include <stack>


NodeKdTree::NodeKdTree ( const shared_ptr<std::vector<Node> >& ns ) {
	log4cpp::Category& log = log4cpp::Category::getRoot();
	log.infoStream() << "Nodes: " << ns->size();

	log.infoStream() << " - creating leaves";
	nodes.reserve(ns->size());
    for (unsigned int i = 0; i < ns->size(); i++ ) {
            boost::shared_ptr<kdNode> nodeKD = boost::make_shared<kdNode>();
            nodeKD->ids.push_back(NodeId(i));
            nodeKD->refPoints.push_back(( *ns ) [i].getLocation());
            nodeKD->left = shared_ptr<kdNode>() ;
            nodeKD->right = shared_ptr<kdNode>() ;
            nodes.push_back ( nodeKD );
    }
}

void NodeKdTree::buildTree() {
	log4cpp::Category& log = log4cpp::Category::getRoot();
	log.infoStream() << " - building tree";
	if (nodes.size() > 0)
		root = buildKDtree ( nodes, 0 );
}

/**
 * This Methode builds the kd-tree
 * it has an vector which stores all kd Tree nodes
 *
 * left and right refers to the child nodes not sides
 */
shared_ptr<NodeKdTree::kdNode> NodeKdTree::buildKDtree ( std::vector<shared_ptr<kdNode> >&  toInsert, int depth ) {

	std::stack<BuildStackEntry> buildStack;
	shared_ptr<kdNode> root = boost::make_shared<kdNode>();
	buildStack.push(BuildStackEntry(root, toInsert, 0));

	do {
		BuildStackEntry se = buildStack.top();
		buildStack.pop();

		shared_ptr<kdNode> newNode = se.node;
		std::vector<shared_ptr<kdNode>>& nodes = se.toInsert;
		int depth = se.depth;

		if ( nodes.size() <= 1024 ) {
			for (unsigned int i = 0; i < nodes.size(); i++) {
				newNode->ids.push_back(nodes[i]->ids[0]);
				newNode->refPoints.push_back(nodes[i]->refPoints[0]);
			}
			continue;
		}

		FixedPoint helpP;
		unsigned int count = 0;
		std::vector<shared_ptr<kdNode>> leftL;
		std::vector<shared_ptr<kdNode>> rightL;
		//To get a balaced tree I need to gain the median x-value or y value
		if ( ( depth % 2 ) == 0 ) {
			std::sort ( nodes.begin(), nodes.end(), &NodeKdTree::operatorSortX ); // refer to x
			helpP =  getMedian ( nodes );
			while ( count < nodes.size() && nodes[count]->refPoints[0].x <= helpP.x) {
				leftL.push_back ( nodes[count] );
				count++;
			}
			while ( count < nodes.size() ) {
				rightL.push_back ( nodes[count] );
				count++;
			}
		} else if ( ( depth % 2 ) == 1 ) {				  //refer to y
			std::sort ( nodes.begin(), nodes.end(), &NodeKdTree::operatorSortY );
			helpP =  getMedian ( nodes );
			while ( count < nodes.size() && nodes[count]->refPoints[0].y <= helpP.y ) {
				leftL.push_back ( nodes[count] );
				count++;
			}

			while ( count < nodes.size() ) {
				rightL.push_back ( nodes[count] );
				count++;
			}
		}
		newNode->refPoints.push_back(helpP);

		if (leftL.size() > 0) {
			newNode->left = boost::make_shared<kdNode>();
			buildStack.push(BuildStackEntry(newNode->left, leftL,depth+1));
		}

		if (rightL.size() > 0) {
			newNode->right = boost::make_shared<kdNode>();
			buildStack.push(BuildStackEntry(newNode->right, rightL, depth+1));
		}

	} while(!buildStack.empty());

	return root;
}

bool NodeKdTree::search ( boost::shared_ptr<std::vector<NodeId> >& result, const FixedRect& searchRect, bool returnOnFirst ) const
{
	if (!root) return false;

	FixedRect globalRect = FixedRect (
		 std::numeric_limits<coord_t>::min(),
		 std::numeric_limits<coord_t>::min(),
		 std::numeric_limits<coord_t>::max(),
		 std::numeric_limits<coord_t>::max()
					  );

	std::stack< SearchStackEntry> stack;
	stack.push( SearchStackEntry(root, globalRect, 0));

	do {
		SearchStackEntry se = stack.top();
		stack.pop();

		FixedRect leftRect = se.rect;
		FixedRect rightRect = se.rect;
		shared_ptr<kdNode> node = se.node;
		int depth = se.depth;

		// no child nodes
		if (!node->left && !node->right) {
			for (unsigned int i = 0; i < node->ids.size(); i++) {
				if (searchRect.contains( node->refPoints[i] )) {
					if (returnOnFirst) return true;
					result->push_back(node->ids[i]);
				}
			}
			continue;
		}

		// at least one child node
		if ( ( depth % 2 ) == 0 ) {
			leftRect.maxX  = node->refPoints[0].x;
			rightRect.minX = node->refPoints[0].x;
		} else {
			leftRect.maxY  = node->refPoints[0].y;
			rightRect.minY = node->refPoints[0].y;
		}

		if (node->left)
		{
			if (searchRect.contains(leftRect))
			{
				if (returnOnFirst) return true;
				getSubTree(result, node->left);
			}
			else if (searchRect.intersects(leftRect)){
				stack.push( SearchStackEntry(node->left, leftRect, depth+1));
			}
		}
		if (node->right)
		{
			if (searchRect.contains(rightRect))
			{
				if (returnOnFirst) return true;
				getSubTree(result, node->right);
			}
			else if (searchRect.intersects(rightRect)) {
				stack.push( SearchStackEntry(node->right, rightRect, depth+1));
			}
		}
	} while (!stack.empty());

	return returnOnFirst ? false : result->size();
}


void NodeKdTree::getSubTree(shared_ptr<std::vector<NodeId> >& result, const shared_ptr<kdNode>& node) const
{
	std::stack<shared_ptr<kdNode>> nodeSubTreeStack;
	nodeSubTreeStack.push(node);

	do {
		shared_ptr<kdNode> subNode = nodeSubTreeStack.top();
		nodeSubTreeStack.pop();

		// leaf node
		if (!subNode->left && !subNode->right) {
			result->insert(result->end(), subNode->ids.begin(), subNode->ids.end());
			continue;
		}

		if(subNode->left)
			nodeSubTreeStack.push(subNode->left);
		if(subNode->right)
			nodeSubTreeStack.push(subNode->right);
	} while (!nodeSubTreeStack.empty());
}

bool NodeKdTree::contains(const FixedRect& rect) const
{
	shared_ptr<std::vector<NodeId> > nodeIDs = boost::make_shared< std::vector<NodeId> >();
	return search(nodeIDs, rect, true);
}

/**
 * This Methode finds the Point of a vector of Points, which is best to split in Order to get a balaced tree
 */
FixedPoint NodeKdTree::getMedian ( const std::vector<shared_ptr<kdNode > > &  points ) {
    FixedPoint helpP;
	if (points.size() < 2)
		return points[0]->refPoints[0];

    if ( points.size() %2 == 0 ) {   // even number of values
        helpP.x = ( points[points.size() / 2 - 1]->refPoints[0].x + points[ points.size() /2]->refPoints[0].x ) /2;			 // takes the Point which is in the Middle of the vector  zB: a,b,c -> b
        helpP.y = ( points[points.size() / 2 - 1]->refPoints[0].y + points[ points.size() /2]->refPoints[0].y ) /2;
    } else if ( points.size() %2 == 1 ) { // uneven number of values	// take the Point right of the middle zB: length: 4  a,b,c,d  4/2  -1 = 1  -> take second element of vector -> b
        helpP.x  = points[ ( points.size() - 1 ) /2 ]->refPoints[0].x;
        helpP.y  = points[ ( points.size() - 1 ) /2 ]->refPoints[0].y;
    }

    return helpP;

}

bool NodeKdTree::operatorSortY ( const shared_ptr<kdNode>& a, const shared_ptr<kdNode>& b ) {
   return (a->refPoints[0].y < b->refPoints[0].y);
}
bool NodeKdTree::operatorSortX ( const shared_ptr<kdNode>& a, const shared_ptr<kdNode>& b ) {
    return (a->refPoints[0].x < b->refPoints[0].x);
}
