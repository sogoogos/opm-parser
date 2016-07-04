/*
  Copyright 2016 Statoil ASA.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <opm/parser/eclipse/Parser/ParseContext.hpp>
#include <opm/parser/eclipse/Parser/MessageContainer.hpp>
#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/Deck/DeckItem.hpp>
#include <opm/parser/eclipse/Deck/DeckKeyword.hpp>
#include <opm/parser/eclipse/Deck/DeckRecord.hpp>
#include <opm/parser/eclipse/Deck/Section.hpp>
#include <opm/parser/eclipse/EclipseState/Eclipse3DProperties.hpp>
#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>
#include <opm/parser/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Completion.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/CompletionSet.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Group.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/TimeMap.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well.hpp>
#include <opm/parser/eclipse/EclipseState/SummaryConfig/SummaryConfig.hpp>

#include <ert/ecl/ecl_smspec.h>

#include <iostream>
#include <algorithm>
#include <array>

namespace Opm {

namespace {
    /* A dummy deck that holds a Summary section with the keyword list that ALL
     * expands to, plus the SUMMARY header
     */
    const Deck ALL_keywords = {
        "SUMMARY",
        "FAQR",  "FAQRG", "FAQT", "FAQTG", "FGIP", "FGIPG", "FGIPL",
        "FGIR",  "FGIT",  "FGOR", "FGPR",  "FGPT", "FOIP",  "FOIPG",
        "FOIPL", "FOIR",  "FOIT", "FOPR",  "FOPT", "FPR",   "FVIR",
        "FVIT",  "FVPR",  "FVPT", "FWCT",  "FWGR", "FWIP",  "FWIR",
        "FWIT",  "FWPR",  "FWPT",
        "GGIR",  "GGIT",  "GGOR", "GGPR",  "GGPT", "GOIR",  "GOIT",
        "GOPR",  "GOPT",  "GVIR", "GVIT",  "GVPR", "GVPT",  "GWCT",
        "GWGR",  "GWIR",  "GWIT", "GWPR",  "GWPT",
        "WBHP",  "WGIR",  "WGIT", "WGOR",  "WGPR", "WGPT",  "WOIR",
        "WOIT",  "WOPR",  "WOPT", "WPI",   "WTHP", "WVIR",  "WVIT",
        "WVPR",  "WVPT",  "WWCT", "WWGR",  "WWIR", "WWIT",  "WWPR",
        "WWPT",
        // ALL will not expand to these keywords yet
        "AAQR",  "AAQRG", "AAQT", "AAQTG"
    };

/*
    When the error handling config says that the error should be
    logged, the handleMissingWell and handleMissingGroup routines
    cheat. Ideally we should have a MessageContainer instance around
    and pass that to the parseContext::handlError() routine. Instead
    we:

    1. We instantiate new MessageContainer() which is just
        immediately dropped to floor, leaving the messages behind.

    2. Print a message on stderr.

    The case of incorrectly/missing well/group names in the SUMMARY
    section did just not seem important enough to warrant the
    refactoring required to pass a mutable proper MessageContainer
    all the way down here.
*/

void handleMissingWell( const ParseContext& parseContext , const std::string& keyword, const std::string& well) {
    std::string msg = std::string("Error in keyword:") + keyword + std::string(" No such well: ") + well;
    MessageContainer msgContainer;
    if (parseContext.get( ParseContext::SUMMARY_UNKNOWN_WELL) == InputError::WARN)
        std::cerr << "ERROR: " << msg << std::endl;

    parseContext.handleError( ParseContext::SUMMARY_UNKNOWN_WELL , msgContainer , msg );
}


void handleMissingGroup( const ParseContext& parseContext , const std::string& keyword, const std::string& group) {
    std::string msg = std::string("Error in keyword:") + keyword + std::string(" No such group: ") + group;
    MessageContainer msgContainer;
    if (parseContext.get( ParseContext::SUMMARY_UNKNOWN_GROUP) == InputError::WARN)
        std::cerr << "ERROR: " << msg << std::endl;

    parseContext.handleError( ParseContext::SUMMARY_UNKNOWN_GROUP , msgContainer , msg );
}

inline void keywordW( std::vector< ERT::smspec_node >& list,
                      const ParseContext& parseContext,
                      const DeckKeyword& keyword,
                      const Schedule& schedule ) {

    const auto type = ECL_SMSPEC_WELL_VAR;

    if( keyword.size() == 0 ||
        !keyword.getDataRecord().getDataItem().hasValue( 0 ) ) {

        for( const auto& well : schedule.getWells() )
            list.emplace_back( type, well->name(), keyword.name() );

        return;
    }

    const auto& item = keyword.getDataRecord().getDataItem();

    for( const std::string& well : item.getData< std::string >() ) {
        if( schedule.hasWell( well ) )
            list.emplace_back( type, well, keyword.name() );
        else
            handleMissingWell( parseContext, keyword.name(), well );
    }
}

inline void keywordG( std::vector< ERT::smspec_node >& list,
                      const ParseContext& parseContext,
                      const DeckKeyword& keyword,
                      const Schedule& schedule ) {

    const auto type = ECL_SMSPEC_GROUP_VAR;

    if( keyword.size() == 0 ||
        !keyword.getDataRecord().getDataItem().hasValue( 0 ) ) {

        for( const auto& group : schedule.getGroups() )
            list.emplace_back( type, group->name(), keyword.name() );

        return;
    }

    const auto& item = keyword.getDataRecord().getDataItem();

    for( const std::string& group : item.getData< std::string >() ) {
        if( schedule.hasGroup( group ) )
            list.emplace_back( ECL_SMSPEC_GROUP_VAR, group, keyword.name() );
        else
            handleMissingGroup( parseContext, keyword.name(), group );
    }
}

inline void keywordF( std::vector< ERT::smspec_node >& list,
                      const DeckKeyword& keyword ) {
    list.emplace_back( keyword.name() );
}

inline std::array< int, 3 > dimensions( const EclipseGrid& grid ) {
    return {{ int( grid.getNX() ), int( grid.getNY() ), int( grid.getNZ() ) }};
}

inline std::array< int, 3 > getijk( const DeckRecord& record,
                                    int offset = 0 ) {
    return {{
        record.getItem( offset + 0 ).get< int >( 0 ) - 1,
        record.getItem( offset + 1 ).get< int >( 0 ) - 1,
        record.getItem( offset + 2 ).get< int >( 0 ) - 1
    }};
}

inline std::array< int, 3 > getijk( const Completion& completion ) {
    return {{ completion.getI(), completion.getJ(), completion.getK() }};
}

inline void keywordB( std::vector< ERT::smspec_node >& list,
                      const DeckKeyword& keyword,
                      std::array< int, 3 > dims ) {
    for( const auto& record : keyword ) {
        auto ijk = getijk( record );
        list.emplace_back( keyword.name(), dims.data(), ijk.data() );
    }
}

inline void keywordR( std::vector< ERT::smspec_node >& list,
                      const DeckKeyword& keyword,
                      const Eclipse3DProperties& props,
                      std::array< int, 3 > dims ) {

    /* RUNSUM is not a region keyword but a directive for how to format and
     * print output. Unfortunately its *recognised* as a region keyword
     * because of its structure and position. Hence the special handling of ignoring it.
     */
    if( keyword.name() == "RUNSUM" ) return;
    if( keyword.name() == "RPTONLY" ) return;

    const auto& item = keyword.getDataRecord().getDataItem();
    const auto regions = item.hasValue( 0 )
                       ? item.getData< int >()
                       : props.getRegions( "FIPNUM" );

    for( const int region : regions )
        list.emplace_back( keyword.name(), dims.data(), region );
}

inline void keywordC( std::vector< ERT::smspec_node >& list,
                      const ParseContext& parseContext,
                      const DeckKeyword& keyword,
                      const Schedule& schedule,
                      std::array< int, 3 > dims ) {

    const auto& keywordstring = keyword.name();
    const auto last_timestep = schedule.getTimeMap()->last();

    for( const auto& record : keyword ) {

        if( record.getItem( 0 ).defaultApplied( 0 ) ) {
            for( const auto& well : schedule.getWells() ) {

                const auto& name = well->name();

                for( const auto& completion : *well->getCompletions( last_timestep ) ) {
                    auto cijk = getijk( *completion );

                    /* well defaulted, block coordinates defaulted */
                    if( record.getItem( 1 ).defaultApplied( 0 ) ) {
                        list.emplace_back( keywordstring, name, dims.data(), cijk.data() );
                    }
                    /* well defaulted, block coordinates specified */
                    else {
                        auto recijk = getijk( record, 1 );
                        if( std::equal( recijk.begin(), recijk.end(), cijk.begin() ) )
                            list.emplace_back( keywordstring, name, dims.data(), cijk.data() );
                    }
                }
            }

        } else {
            const auto& name = record.getItem( 0 ).get< std::string >( 0 );
            if( !schedule.hasWell( name ) ) {
                handleMissingWell( parseContext, keyword.name(), name );
                continue;
            }

            /* all specified */
            if( !record.getItem( 1 ).defaultApplied( 0 ) ) {
                auto ijk = getijk( record, 1 );
                list.emplace_back( keywordstring, name, dims.data(), ijk.data() );
            }
            else {
                const auto& well = *schedule.getWell( name );
                /* well specified, block coordinates defaulted */
                for( const auto& completion : *well.getCompletions( last_timestep ) ) {
                    auto ijk = getijk( *completion );
                    list.emplace_back( keywordstring, name, dims.data(), ijk.data() );
                }
            }
        }
    }
}

inline void handleKW( std::vector< ERT::smspec_node >& list,
                      const DeckKeyword& keyword,
                      const Schedule& schedule,
                      const Eclipse3DProperties& props,
                      const ParseContext& parseContext,
                      std::array< int, 3 > n_xyz ) {
    const auto var_type = ecl_smspec_identify_var_type( keyword.name().c_str() );

    switch( var_type ) {
        case ECL_SMSPEC_WELL_VAR: return keywordW( list, parseContext, keyword, schedule );
        case ECL_SMSPEC_GROUP_VAR: return keywordG( list, parseContext, keyword, schedule );
        case ECL_SMSPEC_FIELD_VAR: return keywordF( list, keyword );
        case ECL_SMSPEC_BLOCK_VAR: return keywordB( list, keyword, n_xyz );
        case ECL_SMSPEC_REGION_VAR: return keywordR( list, keyword, props, n_xyz );
        case ECL_SMSPEC_COMPLETION_VAR: return keywordC( list, parseContext, keyword, schedule, n_xyz );

        default: return;
    }
}

}

SummaryConfig::SummaryConfig( const Deck& deck, const EclipseState& es , const ParseContext& parseContext)
    : SummaryConfig( deck,
                        *es.getSchedule(),
                        es.get3DProperties(),
                        parseContext,
                        dimensions( *es.getInputGrid() ) )
{}

SummaryConfig::SummaryConfig( const Deck& deck,
                                const Schedule& schedule,
                                const Eclipse3DProperties& props,
                                const ParseContext& parseContext,
                                std::array< int, 3 > n_xyz ) {

    SUMMARYSection section( deck );
    for( auto& x : section )
        handleKW( this->keywords, x, schedule, props, parseContext, n_xyz );

    if( section.hasKeyword( "ALL" ) )
        this->merge( { ALL_keywords, schedule, props, parseContext, n_xyz } );
}

SummaryConfig::const_iterator SummaryConfig::begin() const {
    return this->keywords.cbegin();
}

SummaryConfig::const_iterator SummaryConfig::end() const {
    return this->keywords.cend();
}

SummaryConfig& SummaryConfig::merge( const SummaryConfig& other ) {
    this->keywords.insert( this->keywords.end(),
                            other.keywords.begin(),
                            other.keywords.end() );
    return *this;
}

SummaryConfig& SummaryConfig::merge( SummaryConfig&& other ) {
    auto begin = std::make_move_iterator( other.keywords.begin() );
    auto end = std::make_move_iterator( other.keywords.end() );
    this->keywords.insert( this->keywords.end(), begin, end );
    other.keywords.clear();

    return *this;
}

}
