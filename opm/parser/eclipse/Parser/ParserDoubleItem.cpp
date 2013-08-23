/*
  Copyright 2013 Statoil ASA.

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


#include <opm/json/JsonObject.hpp>

#include <opm/parser/eclipse/Parser/ParserItem.hpp>
#include <opm/parser/eclipse/Parser/ParserDoubleItem.hpp>
#include <opm/parser/eclipse/Parser/ParserEnums.hpp>

#include <opm/parser/eclipse/RawDeck/RawRecord.hpp>
#include <opm/parser/eclipse/Deck/DeckDoubleItem.hpp>

namespace Opm {

    ParserDoubleItem::ParserDoubleItem(const std::string& itemName, ParserItemSizeEnum sizeType) : ParserItem(itemName, sizeType) {
        m_default = defaultDouble();
    }

    ParserDoubleItem::ParserDoubleItem(const std::string& itemName, ParserItemSizeEnum sizeType, double defaultValue) : ParserItem(itemName, sizeType) {
        m_default = defaultValue;
    }

    ParserDoubleItem::ParserDoubleItem(const Json::JsonObject& jsonConfig) : ParserItem(jsonConfig) {
        if (jsonConfig.has_item("default"))
            m_default = jsonConfig.get_double("default");
        else
            m_default = defaultDouble();
    }

    DeckItemConstPtr ParserDoubleItem::scan(RawRecordPtr rawRecord) const {
        if (sizeType() == SINGLE)
            return scan__(false, rawRecord);
        else if (sizeType() == ALL)
            return scan__(true, rawRecord);
        else
            throw std::invalid_argument("Unsupported size type, only support SINGLE and ALL. Use scan( numTokens , rawRecord) instead ");
    }

    /// Scans the rawRecords data according to the ParserItems definition.
    /// returns a DeckItem object.
    /// NOTE: data are popped from the rawRecords deque!

    DeckItemConstPtr ParserDoubleItem::scan__(bool scanAll, RawRecordPtr rawRecord) const {
        DeckDoubleItemPtr deckItem(new DeckDoubleItem(name()));

        bool defaultActive;
        std::deque<double> doublesPreparedForDeckItem = readFromRawRecord(rawRecord, scanAll, m_default, defaultActive);

        if (scanAll)
            deckItem->push_back(doublesPreparedForDeckItem);
        else {
            deckItem->push_back(doublesPreparedForDeckItem.front());
            doublesPreparedForDeckItem.pop_front();
            pushBackToRecord(rawRecord, doublesPreparedForDeckItem, defaultActive);
        }
        return deckItem;
    }

}


