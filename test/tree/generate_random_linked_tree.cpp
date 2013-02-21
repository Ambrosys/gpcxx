/*
 * generate_random_linked_tree.cpp
 * Date: 2013-02-21
 * Author: Karsten Ahnert (karsten.ahnert@gmx.de)
 */

#include <gp/tree/generate_random_linked_tree.hpp>
#include <gp/tree/linked_node_tree.hpp>

#include "../common/test_generator.hpp"

#include <random>

#include <gtest/gtest.h>


TEST( TestName , TestCase )
{
    test_generator gen;

    std::mt19937 rng;

    for( size_t i=0 ; i<1000 ; ++i )
    {
        gp::linked_node_tree< std::string > tree;
        generate_random_linked_tree( tree , gen.gen0 , gen.gen1 , gen.gen2 , rng , 2 , 4 );
        EXPECT_TRUE( tree.data()->height >= 2 );
        EXPECT_TRUE( tree.data()->height <= 4 );
    }
}

