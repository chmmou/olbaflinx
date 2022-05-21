# -*- coding: utf-8 -*-
"""
ktoblzcheck

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""

import sqlite3
import codecs
import argparse

from os.path import isfile, getsize


def createTable():
    """ Create table structure
    """
    cursor = db.cursor()
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS institutions ("
        " country CHAR(2) DEFAULT 'DE' CONSTRAINT germanCountryCode NOT NULL CHECK(country == 'DE'),"
        " bankcode CHAR(8) NOT NULL PRIMARY KEY CHECK(length(bankcode) = 8),"
        " bic CHAR(11),"
        " method CHAR(2),"
        " name VARCHAR(60),"
        " location VARCHAR(40),"
        " valid_upto real"
        " )"
    )
    db.commit()


def formatDate(date):
    """ Formats the date passed to make it acceptable to SQLite julianday()
    """
    tmplist = date.split('.')
    newDate = ""
    return newDate + tmplist[2] + '-' + tmplist[1] + '-' + tmplist[0]


def existDB(file):
    """ Checks if the database file already exists
    """
    if not isfile(file):
        return False

    if getsize(file) < 100:  # Header of a SQLite DB is 100 bytes
        return False

    db = open(file, 'rb')
    header = db.read(100)
    if header.startswith(b'SQLite format 3'):
        return True

    return False


def processFile(fileName, valid_date, generateDB=False):
    """ Updates the database with institutions saved in fileName
    """

    rowsInserted = 0
    rowsUpdated = 0

    cursor = db.cursor()
    cursor.execute("BEGIN")

    def existCode(bankCode):
        cursor.execute("SELECT bankcode FROM institutions WHERE bankcode = ?", (bankCode,))
        row_exist = cursor.fetchone()
        if row_exist is None:
            return False

        return True

    def submitInstitute(bankCode, method, bankName, bic, location):
        """ Add an institution entry in DB
        """
        try:
            cursor.execute("INSERT INTO institutions (bankcode, bic, method, name, location, valid_upto)\
                         VALUES(?,?,?,?,?,julianday(?))",
                           (bankCode, bic, method, bankName, location, None))

        except sqlite3.Error as e:
            print("Error: {0} while inserting {1} ({2})".format(e.args[0], bankCode, bic))

    def deleteInstitute(oldBankCode, newBankCode, method, bankName, bic, location, valid_upto):
        """ Delete an entry from the DB
        """
        try:
            cursor.execute("UPDATE institutions SET valid_upto = julianday(?) WHERE bankcode = ?",
                           (valid_upto, oldBankCode))
            if (newBankCode != '00000000' and existCode(newBankCode) == False):
                cursor.execute("INSERT INTO institutions (bankcode, bic, method, name, location, valid_upto)\
                             VALUES(?,?,?,?,?,julianday(?))",
                               (newBankCode, bic, method, bankName, location, None))
                return 1
            return 0

        except sqlite3.Error as e:
            print("Error: {0} while inserting {1} {2} {3} {4}".format(e.args[0], oldBankCode,
                                                                      newBankCode, location,
                                                                      valid_upto))

    institutesFile = codecs.open(fileName, "r", encoding=args.encoding)
    for institute in institutesFile:
        if institute[8:9] == "1":
            if not existCode(institute[0:8]):
                submitInstitute(institute[0:8],
                                institute[150:152],
                                institute[9:67].strip(),
                                institute[139:150],
                                institute[72:107])
                rowsInserted += 1
            if institute[158] == 'D':
                rowsInserted += deleteInstitute(institute[0:8],
                                                institute[160:168],
                                                institute[150:152],
                                                institute[9:67].strip(),
                                                institute[139:150],
                                                institute[72:107],
                                                valid_date)
                rowsUpdated += 1

    db.commit()
    return (rowsUpdated, rowsInserted)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Creates a SQLite database for KtoBLZCheck with information about IBAN and BICs based on a fixed-column text file from the german central bank."
                    " You can download the source file at https://www.bundesbank.de/de/aufgaben/unbarer-zahlungsverkehr/serviceangebot/bankleitzahlen/download-bankleitzahlen-602592"
    )
    parser.add_argument(dest='file', help='File to load')
    parser.add_argument(dest='valid_date', help='Date until deletions were valid')
    parser.add_argument('-o', '--output', default="bankdata.de.db",
                        help='SQLite database to open/generate')
    parser.add_argument('-e', '--encoding', default="iso 8859-1", help='Charset of file')
    args = parser.parse_args()

    print("Read data from \"{0}\" with \"{1}\" encoding".format(args.file, args.encoding))
    generateDB = False

    if not existDB(args.output):
        generateDB = True

    db = sqlite3.connect(args.output)

    createTable()
    args.valid_date = formatDate(args.valid_date)
    (rowsUpdated, rowsInserted) = processFile(args.file, args.valid_date, generateDB)

    print("Inserted {0} institutions into database \"{1}\" at date {2}".format(rowsInserted,
                                                                               args.output,
                                                                               args.valid_date))

    print("Updated {0} institutions into database \"{1}\" at date {2}".format(rowsUpdated,
                                                                              args.output,
                                                                              args.valid_date))

    cursor = db.cursor()
    cursor.execute("ANALYZE institutions")
    if generateDB:
        cursor.execute("CREATE INDEX bic_index ON institutions (bic)")
        cursor.execute("CREATE INDEX name_index ON institutions (name)")
        cursor.execute("CREATE INDEX location_index ON institutions (location)")
        cursor.execute("CREATE INDEX valid_upto_index ON institutions (valid_upto)")
    cursor.execute("REINDEX")
    cursor.execute("VACUUM")
    db.commit()
    db.close()
