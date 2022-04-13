#!/usr/local/evn python

import argparse
import sys

from datetime import date

class DBField(object):

    def __init__(self, data):
        data = data.split(';')
        self._code = int(data[0], 16)
        self._name = data[1]
        self._category = data[2]
        self._combining_classes = data[3]
        self._bidirectional_category = data[4]
        self._decomposition_mapping = data[5]
        self._decimal_digit_value = data[6]
        self._digit_value = data[7]
        self._numeric_value = data[8]
        self._mirrored = data[9]
        self._unicode_name = data[10]
        self._comment = data[11]
        if data[12]:
            self._uppercase_mapping = int(data[12], 16)
        else:
            self._uppercase_mapping = None
        if data[13]:
            self._lowercase_mapping = int(data[13], 16)
        else:
            self._lowercase_mapping = None
        if data[14]:
            self._titlecase_mapping = int(data[14], 16)
        else:
            self._titlecase_mapping = None

    def code(self):
        return self._code

    def category(self):
        return self._category

    def upper(self):
        return self._uppercase_mapping

    def lower(self):
        return self._lowercase_mapping

    def title_case(self):
        return self._titlecase_mapping


def parse_args():
    parser = argparse.ArgumentParser(description='Unicode table C file')
    parser.add_argument('db', type=argparse.FileType('r'), help='UnicodeData.txt file')
    parser.add_argument('univer')
    return parser.parse_args()

def add_file_header(buf, ver):
    buf.append('/* The MIT License (MIT)')
    buf.append(' * ')
    buf.append(' * Copyright (c) {year} Monetra Technologies, LLC.'.format(year=date.today().year))
    buf.append(' * ')
    buf.append(' * Permission is hereby granted, free of charge, to any person obtaining a copy')
    buf.append(' * of this software and associated documentation files (the "Software"), to deal')
    buf.append(' * in the Software without restriction, including without limitation the rights')
    buf.append(' * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell')
    buf.append(' * copies of the Software, and to permit persons to whom the Software is')
    buf.append(' * furnished to do so, subject to the following conditions:')
    buf.append(' * ')
    buf.append(' * The above copyright notice and this permission notice shall be included in')
    buf.append(' * all copies or substantial portions of the Software.')
    buf.append(' * ')
    buf.append(' * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR')
    buf.append(' * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,')
    buf.append(' * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE')
    buf.append(' * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER')
    buf.append(' * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,')
    buf.append(' * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN')
    buf.append(' * THE SOFTWARE.')
    buf.append(' */')
    buf.append('')
    buf.append('#include "m_utf8_int.h"')
    buf.append('')
    buf.append('/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -')
    buf.append(' * Mapping tables generated from Unicode {unidataver} UnicodeData.txt.'.format(unidataver=ver))
    buf.append(' * ')
    buf.append(' * All element in tables _must_ be in sorted order low to high. Access of')
    buf.append(' * the tables uses a binary search which will only work if the tables are sorted.')
    buf.append('*/')
    buf.append('')
    buf.append('/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */')
    buf.append('')

def build_tables(buf, tables):
    max_perline = 10

    names = sorted(tables.keys())
    for name in names:
        table = tables.get(name)
        buf.append('const M_uint32 M_utf8_table_{name}[] = {{'.format(name=name))
        cps = sorted(table)
        l = [ cps[i:i+max_perline] for i in range(0, len(cps), max_perline) ]
        for i, x in enumerate(l):
            line = ', '.join([ '0x{val:04X}'.format(val=c) for c in x ])
            buf.append('\t{line}{end_comma}'.format(line=line, end_comma=',' if i<len(l)-1 else ''))
        buf.append('};')
        buf.append('const size_t M_utf8_table_{name}_len = {cnt};'.format(name=name, cnt=len(table)))

        buf.append('')
        buf.append('/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */')
        buf.append('')

def _build_mapped(buf, codes, var_name, comment):
    max_perline = 5

    buf.append('/* {comment} */'.format(comment=comment))
    buf.append('const M_utf8_cp_map_t M_utf8_table_{var_name}[] = {{'.format(var_name=var_name))

    cps = sorted(codes.keys())
    l = [ cps[i:i+max_perline] for i in range(0, len(cps), max_perline) ]
    for i, x in enumerate(l):
        line = ', '.join([ '{{ 0x{val1:04X}, 0x{val2:04X} }}'.format(val1=c, val2=codes.get(c)) for c in x ])
        buf.append('\t{line}{end_comma}'.format(line=line, end_comma=',' if i<len(l)-1 else ''))
    buf.append('};')
    buf.append('const size_t M_utf8_table_{var_name}_len = {cnt};'.format(var_name=var_name, cnt=len(codes)))

    buf.append('')
    buf.append('/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */')
    buf.append('')

def build_upper_map(buf, upper_map):
    _build_mapped(buf, upper_map, 'uptolow', 'idx 0 = upper cp, idx 1 = lower cp')

def build_lower_map(buf, lower_map):
    _build_mapped(buf, lower_map, 'lowtoup', 'idx 0 = lower cp, idx 1 = upper cp')

def build_title_map(buf, title_map):
    _build_mapped(buf, title_map, 'title', 'idx 0 = lower cp, idx 1 = upper cp')

def main():
    args = parse_args()
    fields = []
    tables = {}
    upper_map = {}
    lower_map = {}
    title_map = {}

    # Generate all of our fields
    for line in args.db:
        fields.append(DBField(line.strip()))

    for field in sorted(fields, key=lambda x: x.code()):
        # Build our tables
        if field.category() not in tables:
            tables[field.category()] = []
        t = tables.get(field.category())
        t.append(field.code())

        if field.upper():
            lower_map[field.code()] = field.upper()

        if field.lower():
            upper_map[field.code()] = field.lower()

        if field.title_case():
            title_map[field.code()] = field.title_case()

    buf = []
    add_file_header(buf, args.univer)
    build_tables(buf, tables)
    build_upper_map(buf, upper_map)
    build_lower_map(buf, lower_map)
    build_title_map(buf, title_map)

    out = '\n'.join(buf)
    print(out)

    return 0

if __name__ == '__main__':
    sys.exit(main())
