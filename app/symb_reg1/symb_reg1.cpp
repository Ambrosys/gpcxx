/*
 * main_optimize.cpp
 * Date: 2013-01-28
 * Author: Karsten Ahnert (karsten.ahnert@gmx.de)
 */

#include "eval.hpp"

#include <gp/ga/ga1.hpp>
#include <gp/tree/linked_node_tree.hpp>
#include <gp/tree/generate_random_linked_tree.hpp>
#include <gp/operator/mutation.hpp>
#include <gp/operator/crossover.hpp>
#include <gp/stat/population_statistics.hpp>
#include <gp/util/log.hpp>

#include <boost/make_shared.hpp>

#include <fstream>
#include <iostream>
#include <random>
#include <vector>

#define tab "\t"

using namespace std;


template< class Rng , class Func >
void generate_test_data( vector_t &y , vector_t &x1 , vector_t &x2 , vector_t &x3 , size_t n , Rng &rng , Func f )
{
    std::normal_distribution< double > dist1( 0.0 , 1.0 ) , dist2( 0.0 , 1.0 ) , dist3( 0.0 , 1.0 );
    x1.resize( n ) ; x2.resize( n ) ; x3.resize( n ) ; y.resize( n ) ;
    for( size_t i = 0 ; i < n ; ++i )
    {
        x1[i] = dist1( rng );
        x2[i] = dist2( rng );
        x3[i] = dist3( rng );
        y[i] = f( x1[i] , x2[i] , x3[i] );
    }
}

std::pair< double , double > normalize( vector_t &x )
{
    double mean = 0.0 , sq_mean = 0.0;
    for( auto d : x )
    {
        mean += d;
        sq_mean += d * d;
    }
    mean /= double( x.size() );
    sq_mean /= double( x.size() );
    double stdev = sqrt( sq_mean - mean * mean );
    for( auto &d : x )
    {
        d = ( d - mean ) / stdev;
    }
    return make_pair( mean , stdev );
}


void normalize( vector_t &x1 , vector_t &x2 , vector_t &x3 )
{
    auto p1 = normalize( x1 );
    auto p2 = normalize( x2 );
    auto p3 = normalize( x3 );
    // cout << p1.first << " " << p1.second << endl;
    // cout << p2.first << " " << p2.second << endl;
    // cout << p3.first << " " << p3.second << endl;
}



namespace pl = std::placeholders;
std::vector< boost::shared_ptr< std::ostream > > streams;

void init_logging( void )
{
    using namespace Amboss::Log;
    LoggerCollection &logger = GlobalLogger::getInstance();
    logger.data().clear();
    
    auto filter = []( const LogEntry &e ) { return ( e.logLevel >= NOISE ); };
    std::shared_ptr< OStreamLogger > l( std::make_shared< OStreamLogger >( std::cerr , gp::DefaultFormatter() , filter ) );
    logger.data().push_back( std::shared_ptr< ILogger >( l ) );

    boost::shared_ptr< std::ostream > s = boost::make_shared< std::ofstream >( "log.dat" );
    streams.push_back( s );
    std::shared_ptr< OStreamLogger > ll = std::make_shared< OStreamLogger >( *s , gp::DefaultFormatter() , filter );
    logger.data().push_back( std::shared_ptr< ILogger >( ll ) );
}

int main( int argc , char *argv[] )
{
    typedef std::mt19937 rng_type ;
    typedef fitness_function::context_type context_type;
    typedef gp::genetic_evolver1< tree_type , fitness_function::context_type , std::mt19937 > evolver_type;

    init_logging();


    rng_type rng;

    context_type c;
    // generate_test_data( c.y , c.x1 , c.x2 , c.x3 , 10000 , rng ,
    //                     []( double x1 , double x2 , double x3 ) { return x1 + x2 - x3; } );
    generate_test_data( c.y , c.x1 , c.x2 , c.x3 , 10000 , rng ,
                        []( double x1 , double x2 , double x3 ) { return x1 * x1 * x1 + 1.0 / 10.0 * x2 * x2 - 3.0 / 4.0 * ( x3 - 4.0 ) + 1.0 ; } );
//    normalize( x1 , x2 , x3 );

    generators< rng_type > gen( rng );

    size_t population_size = 5000;
    double elite_rate = double( 2 ) / double( population_size );
    double mutation_rate = 0.2;
    double crossover_rate = 0.6;
    size_t min_tree_height = 4 , max_tree_height = 8;

    std::function< void( tree_type& ) > tree_generator;
    std::array< int , 3 > weights = {{ 2 * int( gen.gen0.num_symbols() ) ,
                                       int( gen.gen1.num_symbols() ) , 
                                       int( gen.gen2.num_symbols() ) }};
    tree_generator = make_tree_generator_binder( rng , gen.gen0 , gen.gen1 , gen.gen2 , min_tree_height , max_tree_height , weights );

    evolver_type evolver( elite_rate , mutation_rate , crossover_rate , rng );
    std::vector< double > fitness( population_size , 0.0 );
    std::vector< tree_type > population( population_size );


    evolver.fitness_function() = fitness_function();
    evolver.mutation_function() = gp::make_mutation_binder( rng , gen.gen0 , gen.gen1 , gen.gen2 );
    evolver.crossover_function() = gp::make_crossover_binder( rng , max_tree_height );
    evolver.random_individual_function() = tree_generator;


    // initialize population with random trees and evaluate fitness
    GP_LOG_LEVEL_MODULE( gp::LogLevel::PROGRESS , gp::MAIN ) << "Starting initialization!";
    for( size_t i=0 ; i<population.size() ; ++i )
    {
        tree_generator( population[i] );
        fitness[i] = fitness_function()( population[i] , c );
    }
    GP_LOG_LEVEL_MODULE( gp::LogLevel::PROGRESS , gp::MAIN ) << "Finishing initialization!";
    
    GP_LOG_LEVEL_MODULE( gp::LogLevel::PROGRESS , gp::MAIN ) << "Starting main loop!";
    for( size_t i=0 ; i<100 ; ++i )
    {
        GP_LOG_LEVEL_MODULE( gp::LogLevel::PROGRESS , gp::MAIN ) << "Starting Iteration " << i << "!";

        evolver.next_generation( population , fitness , c );

        std::vector< size_t > idx;
        auto iter = gp::sort_indices( fitness , idx );
        for( size_t j=0 ; j<10 ; ++j )
            GP_LOG_LEVEL_MODULE( gp::LogLevel::PROGRESS , gp::MAIN )
                << "Individual " << j << " : " << fitness[ idx[j] ] << " : " << gp::simple( population[ idx[j] ] );
        GP_LOG_LEVEL_MODULE( gp::LogLevel::PROGRESS , gp::MAIN ) << "Finishing Iteration " << i << "!";
    }
    GP_LOG_LEVEL_MODULE( gp::LogLevel::PROGRESS , gp::MAIN ) << "Finishing main loop!";

    return 0;
}
