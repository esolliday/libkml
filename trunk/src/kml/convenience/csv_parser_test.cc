// Copyright 2010, Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file contains the unit tests for the CsvParser class.

#include "kml/convenience/csv_parser.h"

#include "boost/scoped_ptr.hpp"
#include "gtest/gtest.h"
#include "kml/base/csv_splitter.h"
#include "kml/base/file.h"
#include "kml/base/string_util.h"
#include "kml/convenience/convenience.h"
#include "kml/engine.h"

// The following define is a convenience for testing inside Google.
#ifdef GOOGLE_INTERNAL
#include "kml/base/google_internal_test.h"
#endif

#ifndef DATADIR
#error *** DATADIR must be defined! ***
#endif

namespace kmlconvenience {

// This CsvParserHandler adds each Placemark to the supplied Container and
// saves each non OK status to an error log if one is supplied.
class ContainerSaver : public CsvParserHandler {
 public:
  typedef std::vector<std::pair<int, CsvParserStatus> > ErrorLog;
  ContainerSaver(kmldom::ContainerPtr container, ErrorLog* log)
    : container_(container), log_(log) {
  }
  virtual bool HandleLine(int line_number, CsvParserStatus status,
                          kmldom::PlacemarkPtr placemark) {
    if (status == CSV_PARSER_STATUS_OK) {
      container_->add_feature(placemark);
    } else if (log_) {
      log_->push_back(std::make_pair(line_number, status));
    }
    return true;
  }

 private:
  kmldom::ContainerPtr container_;
  ErrorLog* log_;
};

// This internal utility function returns true iff the given <Placemark> has
// a <Point> with the given latitude and longitude.
static bool CheckPointLatLon(const kmldom::PlacemarkPtr& placemark,
                             double want_lat, double want_lon) {
  if (placemark && placemark->has_geometry()) {
    if (kmldom::PointPtr point = kmldom::AsPoint(placemark->get_geometry())) {
      double got_lat;
      double got_lon;
      return kmlengine::GetPointLatLon(point, &got_lat, &got_lon) &&
             want_lat == got_lat && want_lon == got_lon;
    }
  }
  return false;
}

// This test verifies that a bad schema is detected.
TEST(CsvParserTest, TestSetSchemaEmpty) {
  CsvParserHandler csv_parser_handler;
  CsvParser csv_parser(NULL, &csv_parser_handler);
  kmlbase::StringVector empty;
  ASSERT_EQ(CSV_PARSER_STATUS_BLANK_LINE, csv_parser.SetSchema(empty));
}

// This test verifies that a minimal schema is accepted.
TEST(CsvParserTest, TestSetSchemaMinimal) {
  CsvParserHandler csv_parser_handler;
  CsvParser csv_parser(NULL, &csv_parser_handler);
  kmlbase::StringVector schema;
  schema.push_back("latitude");
  schema.push_back("longitude");
  ASSERT_EQ(CSV_PARSER_STATUS_OK, csv_parser.SetSchema(schema));
}

// This test verifies that a schema with <ExtendedData> items is accepted.
TEST(CsvParserTest, TestSetSchemaExtraCols) {
  kmlbase::CsvSplitter schema_csv("latitude,longitude,par,yardage\n");
  kmlbase::StringVector schema;
  schema_csv.SplitCurrentLine(&schema);
  CsvParserHandler csv_parser_handler;
  CsvParser csv_parser(&schema_csv, &csv_parser_handler);
  ASSERT_EQ(CSV_PARSER_STATUS_OK, csv_parser.SetSchema(schema));
  const CsvParser::CsvSchema& csv_schema = csv_parser.GetSchema();
  ASSERT_EQ(static_cast<size_t>(2), csv_schema.size());
  ASSERT_EQ(string("par"), csv_schema.find(2)->second);
  ASSERT_EQ(string("yardage"), csv_schema.find(3)->second);
}

// This test verifies that a schema with mixed case names is accepted.
TEST(CsvParserTest, TestSetSchemaMixedCase) {
  kmlbase::CsvSplitter schema_csv(
                "Name,Longitude,Latitude,BlueYardage,WhiteYardage,"
                "MensHandicap,MensPar,RedYardage,WomensHandicap,WomensPar\n");
  kmlbase::StringVector schema;
  schema_csv.SplitCurrentLine(&schema);
  CsvParserHandler csv_parser_handler;
  CsvParser csv_parser(&schema_csv, &csv_parser_handler);
  ASSERT_EQ(CSV_PARSER_STATUS_OK, csv_parser.SetSchema(schema));
  const CsvParser::CsvSchema& csv_schema = csv_parser.GetSchema();
  ASSERT_EQ(static_cast<size_t>(7), csv_schema.size());
  ASSERT_TRUE(csv_schema.end() == csv_schema.find(0));
  ASSERT_EQ("BlueYardage", csv_schema.find(3)->second);
  ASSERT_EQ("WhiteYardage", csv_schema.find(4)->second);
  ASSERT_EQ("MensHandicap", csv_schema.find(5)->second);
  ASSERT_EQ("MensPar", csv_schema.find(6)->second);
  ASSERT_EQ("RedYardage", csv_schema.find(7)->second);
  ASSERT_EQ("WomensHandicap", csv_schema.find(8)->second);
  ASSERT_EQ("WomensPar", csv_schema.find(9)->second);
}

// This test verifies full parse of a simple single line CSV file.
TEST(CsvParserTest, TestParseCsvDataOneLine) {
  kmlbase::CsvSplitter csv_data("name,latitude,longitude\n"
                                "hello,38.1,-121.2\n");
  kmldom::FolderPtr folder = kmldom::KmlFactory::GetFactory()->CreateFolder();
  ContainerSaver container_saver(folder, NULL);
  ASSERT_TRUE(CsvParser::ParseCsv(&csv_data, &container_saver));
  ASSERT_EQ(static_cast<size_t>(1), folder->get_feature_array_size());
  ASSERT_TRUE(CheckPointLatLon(
      kmldom::AsPlacemark(folder->get_feature_array_at(0)), 38.1, -121.2));
}

// This test verifies full parse of a simple single line CSV file using
// all "built-in" fields.
TEST(CsvParserTest, TestCsvLineToPlacemarkWithNameAndDescription) {
  const string kName("Hi there");
  const string kDescription("How are you?");
  const double kLat(38.123);
  const double kLon(-123.125);
  kmlbase::CsvSplitter csv_data(
    string("name,latitude,longitude,description\n") + kName + "," +
    kmlbase::ToString(kLat) + "," + kmlbase::ToString(kLon) + "," +
    kDescription);
  kmldom::FolderPtr folder = kmldom::KmlFactory::GetFactory()->CreateFolder();
  ContainerSaver container_saver(folder, NULL);
  ASSERT_TRUE(CsvParser::ParseCsv(&csv_data, &container_saver));
  ASSERT_EQ(static_cast<size_t>(1), folder->get_feature_array_size());
  kmldom::PlacemarkPtr placemark =
      kmldom::AsPlacemark(folder->get_feature_array_at(0));
  ASSERT_TRUE(placemark);
  ASSERT_TRUE(CheckPointLatLon(placemark, kLat, kLon));
  ASSERT_TRUE(placemark->has_name());
  ASSERT_EQ(kName, placemark->get_name());
  ASSERT_TRUE(placemark->has_description());
  ASSERT_EQ(kDescription, placemark->get_description());
}

// This test verifies full parse of a simple single line CSV file using
// all "built-in" fields and some <ExtendedData> fields.
TEST(CsvParserTest, TestCsvLineToPlacemarkWithExtendedData) {
  const string kName("Hi there");
  const double kLat(38.123);
  const double kLon(-123.125);
  const string kWid("42");
  const string kHt("1001");
  kmlbase::CsvSplitter csv_data(
      string("name,longitude,latitude,wid,ht\n") + kName + "," +
      kmlbase::ToString(kLon) + "," + kmlbase::ToString(kLat) + "," +
      kWid + "," + kHt);
  kmldom::FolderPtr folder = kmldom::KmlFactory::GetFactory()->CreateFolder();
  ContainerSaver container_saver(folder, NULL);
  ASSERT_TRUE(CsvParser::ParseCsv(&csv_data, &container_saver));
  ASSERT_EQ(static_cast<size_t>(1), folder->get_feature_array_size());
  kmldom::PlacemarkPtr placemark =
      kmldom::AsPlacemark(folder->get_feature_array_at(0));
  ASSERT_TRUE(CheckPointLatLon(placemark, kLat, kLon));
  ASSERT_TRUE(placemark->has_extendeddata());
  const kmldom::ExtendedDataPtr& ed = placemark->get_extendeddata();
  ASSERT_EQ(static_cast<size_t>(2), ed->get_data_array_size());
  ASSERT_EQ(string("wid"), ed->get_data_array_at(0)->get_name());
  ASSERT_EQ(kWid, ed->get_data_array_at(0)->get_value());
  ASSERT_EQ(string("ht"), ed->get_data_array_at(1)->get_name());
  ASSERT_EQ(kHt, ed->get_data_array_at(1)->get_value());
}

// This verifies the CsvParser on a test file.
TEST(CsvParserTest, TestLincolnParkGc) {
  kmldom::FolderPtr folder = kmldom::KmlFactory::GetFactory()->CreateFolder();
  ContainerSaver container_saver(folder, NULL);
  string csv_data;
  ASSERT_TRUE(kmlbase::File::ReadFileToString(
      kmlbase::File::JoinPaths(DATADIR,
          kmlbase::File::JoinPaths("csv", "lincoln-park-gc.csv")),
      &csv_data));
  kmlbase::CsvSplitter csv_splitter(csv_data);
  ASSERT_TRUE(CsvParser::ParseCsv(&csv_splitter, &container_saver));
  ASSERT_EQ(static_cast<size_t>(18), folder->get_feature_array_size());
  double lat_sum = 0;
  double lon_sum = 0;
  for (size_t i = 0; i < 18; ++i) {
    const kmldom::PlacemarkPtr& p =
        kmldom::AsPlacemark(folder->get_feature_array_at(i));
    ASSERT_TRUE(p);
    ASSERT_EQ(kmlbase::ToString(i+1), p->get_name());
    ASSERT_TRUE(p->has_extendeddata());
    const kmldom::ExtendedDataPtr ed = p->get_extendeddata();
    ASSERT_EQ(static_cast<size_t>(7), ed->get_data_array_size());
    double lat, lon;
    ASSERT_TRUE(kmlengine::GetFeatureLatLon(p, &lat, &lon));
    lat_sum += lat;
    lon_sum += lon;
    ASSERT_EQ("BlueYardage", ed->get_data_array_at(0)->get_name());
    ASSERT_EQ("WhiteYardage", ed->get_data_array_at(1)->get_name());
    ASSERT_EQ("MensHandicap", ed->get_data_array_at(2)->get_name());
    ASSERT_EQ("MensPar", ed->get_data_array_at(3)->get_name());
    ASSERT_EQ("RedYardage", ed->get_data_array_at(4)->get_name());
    ASSERT_EQ("WomensHandicap", ed->get_data_array_at(5)->get_name());
    ASSERT_EQ("WomensPar", ed->get_data_array_at(6)->get_name());
  }
  ASSERT_DOUBLE_EQ(680.115712, lat_sum);
  ASSERT_DOUBLE_EQ(-2204.970429, lon_sum);
}

// This verifies that a bad data line is detected.
TEST(CsvParserTest, TestBadLineError) {
  kmldom::FolderPtr folder = kmldom::KmlFactory::GetFactory()->CreateFolder();
  ContainerSaver::ErrorLog log;
  ContainerSaver container_saver(folder, &log);
  kmlbase::CsvSplitter csv_splitter("latitude,longitude\n"
                                    "this,is,bad\n"
                                    "1.1,-2.2\n");
  ASSERT_TRUE(CsvParser::ParseCsv(&csv_splitter, &container_saver));
  ASSERT_EQ(static_cast<size_t>(1), log.size());
  ASSERT_EQ(2, log[0].first);
  ASSERT_EQ(CSV_PARSER_STATUS_INVALID_DATA, log[0].second);
  ASSERT_EQ(static_cast<size_t>(1), folder->get_feature_array_size());
  kmldom::PlacemarkPtr placemark =
      kmldom::AsPlacemark(folder->get_feature_array_at(0));
  ASSERT_TRUE(CheckPointLatLon(placemark, 1.1, -2.2));
}

}  // end namespace kmlconvenience

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
