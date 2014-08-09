/* Copyright (C) 2014 J.F.Dockes
 *       This program is free software; you can redistribute it and/or modify
 *       it under the terms of the GNU General Public License as published by
 *       the Free Software Foundation; either version 2 of the License, or
 *       (at your option) any later version.
 *
 *       This program is distributed in the hope that it will be useful,
 *       but WITHOUT ANY WARRANTY; without even the implied warranty of
 *       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *       GNU General Public License for more details.
 *
 *       You should have received a copy of the GNU General Public License
 *       along with this program; if not, write to the
 *       Free Software Foundation, Inc.,
 *       59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <string>
#include <unordered_map>
using namespace std;

#include "libupnpp/expatmm.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/control/avlastchg.hxx"

namespace UPnPClient {


class LastchangeParser : public expatmm::inputRefXMLParser {
public:
    LastchangeParser(const string& input, unordered_map<string,string>& props)
        : expatmm::inputRefXMLParser(input), m_props(props)
        {}

protected:
    virtual void StartElement(const XML_Char *name, const XML_Char **attrs)
    {
        //LOGDEB("LastchangeParser: begin " << name << endl);
        for (int i = 0; attrs[i] != 0; i += 2) {
            //LOGDEB("    " << attrs[i] << " -> " << attrs[i+1] << endl);
            if (!strcmp("val", attrs[i])) {
                m_props[name] = attrs[i+1];
            }
        }
    }
private:
    unordered_map<string, string>& m_props;
};


bool decodeAVLastChange(const string& xml, 
                        unordered_map<string, string>& props)
{
    LastchangeParser mparser(xml, props);
    if (!mparser.Parse())
        return false;
    return true;
}


}
