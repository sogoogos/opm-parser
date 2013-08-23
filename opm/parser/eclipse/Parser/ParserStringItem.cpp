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

#include <opm/parser/eclipse/Parser/ParserStringItem.hpp>
#include <opm/parser/eclipse/Deck/DeckStringItem.hpp>

namespace Opm {

    ParserStringItem::ParserStringItem(const std::string& itemName, ParserItemSizeEnum sizeType) : ParserItem(itemName, sizeType) {
        m_default = defaultString();
    }

    ParserStringItem::ParserStringItem(const std::string& itemName, ParserItemSizeEnum sizeType, std::string defaultValue) : ParserItem(itemName, sizeType) {
        m_default = defaultValue;
    }

    ParserStringItem::ParserStringItem(const Json::JsonObject& jsonConfig) : ParserItem(jsonConfig) {
        if (jsonConfig.has_item("default"))
            m_default = jsonConfig.get_string("default");
        else
            m_default = defaultString();
    }

    DeckItemConstPtr ParserStringItem::scan(RawRecordPtr rawRecord) const {
        if (sizeType() == SINGLE) {
            return scan__(false, rawRecord);
        } else if (sizeType() == ALL) {
            return scan__(true, rawRecord);
        } else
            throw std::invalid_argument("Unsupported size type, only support SINGLE and ALL. Use scan( numTokens , rawRecord) instead ");
    }

    /// Scans the rawRecords data according to the ParserItems definition.
    /// returns a DeckItem object.
    /// NOTE: data are popped from the rawRecords deque!

    DeckItemConstPtr ParserStringItem::scan__(bool scanAll, RawRecordPtr rawRecord) const {
        DeckStringItemPtr deckItem(new DeckStringItem(name()));

        bool defaultActive;
        std::deque<std::string> stringsPreparedForDeckItem = readFromRawRecord(rawRecord, scanAll, m_default, defaultActive);
        if (scanAll)
            deckItem->push_back(stringsPreparedForDeckItem);
        else {
            deckItem->push_back(stringsPreparedForDeckItem.front());
            stringsPreparedForDeckItem.pop_front();
            pushBackToRecord(rawRecord, stringsPreparedForDeckItem, defaultActive);
        }
        return deckItem;
    }
}
