/*
 * Copyright 2004-present Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <folly/String.h>
#include <folly/dynamic.h>
#include <folly/experimental/logging/LogCategory.h>
#include <folly/experimental/logging/LogConfig.h>
#include <folly/experimental/logging/LogConfigParser.h>
#include <folly/json.h>
#include <folly/portability/GMock.h>
#include <folly/portability/GTest.h>
#include <folly/test/TestUtils.h>

using namespace folly;

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace folly {
std::ostream& operator<<(std::ostream& os, const LogCategoryConfig& config) {
  os << logLevelToString(config.level);
  if (!config.inheritParentLevel) {
    os << "!";
  }
  if (config.handlers.hasValue()) {
    os << ":" << join(",", config.handlers.value());
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const LogHandlerConfig& config) {
  os << config.type;
  bool first = true;
  for (const auto& opt : config.options) {
    if (!first) {
      os << ",";
    } else {
      os << ":";
      first = false;
    }
    os << opt.first << "=" << opt.second;
  }
  return os;
}
} // namespace folly

TEST(LogConfig, parseBasic) {
  auto config = parseLogConfig("");
  EXPECT_THAT(config.getCategoryConfigs(), UnorderedElementsAre());
  EXPECT_THAT(config.getHandlerConfigs(), UnorderedElementsAre());

  config = parseLogConfig("   ");
  EXPECT_THAT(config.getCategoryConfigs(), UnorderedElementsAre());
  EXPECT_THAT(config.getHandlerConfigs(), UnorderedElementsAre());

  config = parseLogConfig(".=ERROR,folly=DBG2");
  EXPECT_THAT(
      config.getCategoryConfigs(),
      UnorderedElementsAre(
          Pair("", LogCategoryConfig{LogLevel::ERR, true}),
          Pair("folly", LogCategoryConfig{LogLevel::DBG2, true})));
  EXPECT_THAT(config.getHandlerConfigs(), UnorderedElementsAre());

  config = parseLogConfig(" INFO , folly  := FATAL   ");
  EXPECT_THAT(
      config.getCategoryConfigs(),
      UnorderedElementsAre(
          Pair("", LogCategoryConfig{LogLevel::INFO, true}),
          Pair("folly", LogCategoryConfig{LogLevel::FATAL, false})));
  EXPECT_THAT(config.getHandlerConfigs(), UnorderedElementsAre());

  config =
      parseLogConfig("my.category:=INFO , my.other.stuff  := 19,foo.bar=DBG7");
  EXPECT_THAT(
      config.getCategoryConfigs(),
      UnorderedElementsAre(
          Pair("my.category", LogCategoryConfig{LogLevel::INFO, false}),
          Pair(
              "my.other.stuff",
              LogCategoryConfig{static_cast<LogLevel>(19), false}),
          Pair("foo.bar", LogCategoryConfig{LogLevel::DBG7, true})));
  EXPECT_THAT(config.getHandlerConfigs(), UnorderedElementsAre());

  config = parseLogConfig(" ERR ");
  EXPECT_THAT(
      config.getCategoryConfigs(),
      UnorderedElementsAre(Pair("", LogCategoryConfig{LogLevel::ERR, true})));
  EXPECT_THAT(config.getHandlerConfigs(), UnorderedElementsAre());

  config = parseLogConfig(" ERR: ");
  EXPECT_THAT(
      config.getCategoryConfigs(),
      UnorderedElementsAre(
          Pair("", LogCategoryConfig{LogLevel::ERR, true, {}})));
  EXPECT_THAT(config.getHandlerConfigs(), UnorderedElementsAre());

  config = parseLogConfig(" ERR:stderr; stderr=file,stream=stderr ");
  EXPECT_THAT(
      config.getCategoryConfigs(),
      UnorderedElementsAre(
          Pair("", LogCategoryConfig{LogLevel::ERR, true, {"stderr"}})));
  EXPECT_THAT(
      config.getHandlerConfigs(),
      UnorderedElementsAre(
          Pair("stderr", LogHandlerConfig{"file", {{"stream", "stderr"}}})));

  config = parseLogConfig(
      "ERR:myfile:custom, folly=DBG2, folly.io:=WARN:other;"
      "myfile=file,path=/tmp/x.log; "
      "custom=custom,foo=bar,hello=world,a = b = c; "
      "other=custom2");
  EXPECT_THAT(
      config.getCategoryConfigs(),
      UnorderedElementsAre(
          Pair(
              "", LogCategoryConfig{LogLevel::ERR, true, {"myfile", "custom"}}),
          Pair("folly", LogCategoryConfig{LogLevel::DBG2, true}),
          Pair(
              "folly.io",
              LogCategoryConfig{LogLevel::WARN, false, {"other"}})));
  EXPECT_THAT(
      config.getHandlerConfigs(),
      UnorderedElementsAre(
          Pair("myfile", LogHandlerConfig{"file", {{"path", "/tmp/x.log"}}}),
          Pair(
              "custom",
              LogHandlerConfig{
                  "custom",
                  {{"foo", "bar"}, {"hello", "world"}, {"a", "b = c"}}}),
          Pair("other", LogHandlerConfig{"custom2"})));

  // Log handler changes with no category changes
  config = parseLogConfig("; myhandler=custom,foo=bar");
  EXPECT_THAT(config.getCategoryConfigs(), UnorderedElementsAre());
  EXPECT_THAT(
      config.getHandlerConfigs(),
      UnorderedElementsAre(
          Pair("myhandler", LogHandlerConfig{"custom", {{"foo", "bar"}}})));
}

TEST(LogConfig, parseBasicErrors) {
  // Errors in the log category settings
  EXPECT_THROW_RE(
      parseLogConfig("=="),
      LogConfigParseError,
      "invalid log level \"=\" for category \"\"");
  EXPECT_THROW_RE(
      parseLogConfig("bogus_level"),
      LogConfigParseError,
      "invalid log level \"bogus_level\" for category \".\"");
  EXPECT_THROW_RE(
      parseLogConfig("foo=bogus_level"),
      LogConfigParseError,
      "invalid log level \"bogus_level\" for category \"foo\"");
  EXPECT_THROW_RE(
      parseLogConfig("foo=WARN,bar=invalid"),
      LogConfigParseError,
      "invalid log level \"invalid\" for category \"bar\"");
  EXPECT_THROW_RE(
      parseLogConfig("foo=WARN,bar="),
      LogConfigParseError,
      "invalid log level \"\" for category \"bar\"");
  EXPECT_THROW_RE(
      parseLogConfig("foo=WARN,bar:="),
      LogConfigParseError,
      "invalid log level \"\" for category \"bar\"");
  EXPECT_THROW_RE(
      parseLogConfig("foo:=,bar:=WARN"),
      LogConfigParseError,
      "invalid log level \"\" for category \"foo\"");
  EXPECT_THROW_RE(
      parseLogConfig("x"),
      LogConfigParseError,
      "invalid log level \"x\" for category \".\"");
  EXPECT_THROW_RE(
      parseLogConfig("x,y,z"),
      LogConfigParseError,
      "invalid log level \"x\" for category \".\"");
  EXPECT_THROW_RE(
      parseLogConfig("foo=WARN,"),
      LogConfigParseError,
      "invalid log level \"\" for category \".\"");
  EXPECT_THROW_RE(
      parseLogConfig("="),
      LogConfigParseError,
      "invalid log level \"\" for category \"\"");
  EXPECT_THROW_RE(
      parseLogConfig(":="),
      LogConfigParseError,
      "invalid log level \"\" for category \"\"");
  EXPECT_THROW_RE(
      parseLogConfig("foo=bar=ERR"),
      LogConfigParseError,
      "invalid log level \"bar=ERR\" for category \"foo\"");
  EXPECT_THROW_RE(
      parseLogConfig("foo.bar=ERR,foo..bar=INFO"),
      LogConfigParseError,
      "category \"foo\\.bar\" listed multiple times under different names: "
      "\"foo\\.+bar\" and \"foo\\.+bar\"");
  EXPECT_THROW_RE(
      parseLogConfig("=ERR,.=INFO"),
      LogConfigParseError,
      "category \"\" listed multiple times under different names: "
      "\"\\.?\" and \"\\.?\"");

  // Errors in the log handler settings
  EXPECT_THROW_RE(
      parseLogConfig("ERR;"),
      LogConfigParseError,
      "error parsing log handler configuration \"\": "
      "expected data in the form NAME=TYPE");
  EXPECT_THROW_RE(
      parseLogConfig("ERR;foo"),
      LogConfigParseError,
      "error parsing log handler configuration \"foo\": "
      "expected data in the form NAME=TYPE");
  EXPECT_THROW_RE(
      parseLogConfig("ERR;foo="),
      LogConfigParseError,
      "error parsing configuration for log handler \"foo\": "
      "empty log handler type");
  EXPECT_THROW_RE(
      parseLogConfig("ERR;=file"),
      LogConfigParseError,
      "error parsing log handler configuration: empty log handler name");
  EXPECT_THROW_RE(
      parseLogConfig("ERR;handler1=file;"),
      LogConfigParseError,
      "error parsing log handler configuration \"\": "
      "expected data in the form NAME=TYPE");
}

TEST(LogConfig, parseJson) {
  auto config = parseLogConfig("{}");
  EXPECT_THAT(config.getCategoryConfigs(), UnorderedElementsAre());
  config = parseLogConfig("  {}   ");
  EXPECT_THAT(config.getCategoryConfigs(), UnorderedElementsAre());

  config = parseLogConfig(R"JSON({
    "categories": {
      ".": "ERROR",
      "folly": "DBG2",
    }
  })JSON");
  EXPECT_THAT(
      config.getCategoryConfigs(),
      UnorderedElementsAre(
          Pair("", LogCategoryConfig{LogLevel::ERR, true}),
          Pair("folly", LogCategoryConfig{LogLevel::DBG2, true})));
  EXPECT_THAT(config.getHandlerConfigs(), UnorderedElementsAre());

  config = parseLogConfig(R"JSON({
    "categories": {
      "": "ERROR",
      "folly": "DBG2",
    }
  })JSON");
  EXPECT_THAT(
      config.getCategoryConfigs(),
      UnorderedElementsAre(
          Pair("", LogCategoryConfig{LogLevel::ERR, true}),
          Pair("folly", LogCategoryConfig{LogLevel::DBG2, true})));
  EXPECT_THAT(config.getHandlerConfigs(), UnorderedElementsAre());

  config = parseLogConfig(R"JSON({
    "categories": {
      ".": { "level": "INFO" },
      "folly": { "level": "FATAL", "inherit": false },
    }
  })JSON");
  EXPECT_THAT(
      config.getCategoryConfigs(),
      UnorderedElementsAre(
          Pair("", LogCategoryConfig{LogLevel::INFO, true}),
          Pair("folly", LogCategoryConfig{LogLevel::FATAL, false})));
  EXPECT_THAT(config.getHandlerConfigs(), UnorderedElementsAre());

  config = parseLogConfig(R"JSON({
    "categories": {
      "my.category": { "level": "INFO", "inherit": true },
      // comments are allowed
      "my.other.stuff": { "level": 19, "inherit": false },
      "foo.bar": { "level": "DBG7" },
    },
    "handlers": {
      "h1": { "type": "custom", "options": {"foo": "bar", "a": "z"} }
    }
  })JSON");
  EXPECT_THAT(
      config.getCategoryConfigs(),
      UnorderedElementsAre(
          Pair("my.category", LogCategoryConfig{LogLevel::INFO, true}),
          Pair(
              "my.other.stuff",
              LogCategoryConfig{static_cast<LogLevel>(19), false}),
          Pair("foo.bar", LogCategoryConfig{LogLevel::DBG7, true})));
  EXPECT_THAT(
      config.getHandlerConfigs(),
      UnorderedElementsAre(Pair(
          "h1", LogHandlerConfig{"custom", {{"foo", "bar"}, {"a", "z"}}})));

  // The JSON config parsing should allow unusual log category names
  // containing whitespace, equal signs, and other characters not allowed in
  // the basic config style.
  config = parseLogConfig(R"JSON({
    "categories": {
      "  my.category  ": { "level": "INFO" },
      " foo; bar=asdf, test": { "level": "DBG1" },
    },
    "handlers": {
      "h1;h2,h3= ": { "type": " x;y " }
    }
  })JSON");
  EXPECT_THAT(
      config.getCategoryConfigs(),
      UnorderedElementsAre(
          Pair("  my.category  ", LogCategoryConfig{LogLevel::INFO, true}),
          Pair(
              " foo; bar=asdf, test",
              LogCategoryConfig{LogLevel::DBG1, true})));
  EXPECT_THAT(
      config.getHandlerConfigs(),
      UnorderedElementsAre(Pair("h1;h2,h3= ", LogHandlerConfig{" x;y "})));
}

TEST(LogConfig, parseJsonErrors) {
  EXPECT_THROW_RE(
      parseLogConfigJson("5"),
      LogConfigParseError,
      "JSON config input must be an object");
  EXPECT_THROW_RE(
      parseLogConfigJson("true"),
      LogConfigParseError,
      "JSON config input must be an object");
  EXPECT_THROW_RE(
      parseLogConfigJson("\"hello\""),
      LogConfigParseError,
      "JSON config input must be an object");
  EXPECT_THROW_RE(
      parseLogConfigJson("[1, 2, 3]"),
      LogConfigParseError,
      "JSON config input must be an object");
  EXPECT_THROW_RE(
      parseLogConfigJson(""), std::runtime_error, "json parse error");
  EXPECT_THROW_RE(
      parseLogConfigJson("{"), std::runtime_error, "json parse error");
  EXPECT_THROW_RE(parseLogConfig("{"), std::runtime_error, "json parse error");
  EXPECT_THROW_RE(
      parseLogConfig("{}}"), std::runtime_error, "json parse error");

  StringPiece input = R"JSON({
    "categories": 5
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "unexpected data type for log categories config: "
      "got integer, expected an object");
  input = R"JSON({
    "categories": {
      "foo": true,
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "unexpected data type for configuration of category \"foo\": "
      "got boolean, expected an object, string, or integer");

  input = R"JSON({
    "categories": {
      "foo": [1, 2, 3],
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "unexpected data type for configuration of category \"foo\": "
      "got array, expected an object, string, or integer");

  input = R"JSON({
    "categories": {
      ".": { "level": "INFO" },
      "folly": { "level": "FATAL", "inherit": 19 },
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "unexpected data type for inherit field of category \"folly\": "
      "got integer, expected a boolean");
  input = R"JSON({
    "categories": {
      "folly": { "level": [], },
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "unexpected data type for level field of category \"folly\": "
      "got array, expected a string or integer");
  input = R"JSON({
    "categories": {
      5: {}
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input), std::runtime_error, "json parse error");

  input = R"JSON({
    "categories": {
      "foo...bar": { "level": "INFO", },
      "foo..bar": { "level": "INFO", },
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "category \"foo\\.bar\" listed multiple times under different names: "
      "\"foo\\.\\.+bar\" and \"foo\\.+bar\"");
  input = R"JSON({
    "categories": {
      "...": { "level": "ERR", },
      "": { "level": "INFO", },
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "category \"\" listed multiple times under different names: "
      "\"(\\.\\.\\.|)\" and \"(\\.\\.\\.|)\"");

  input = R"JSON({
    "categories": { "folly": { "level": "ERR" } },
    "handlers": 9.8
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "unexpected data type for log handlers config: "
      "got double, expected an object");

  input = R"JSON({
    "categories": { "folly": { "level": "ERR" } },
    "handlers": {
      "foo": "test"
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "unexpected data type for configuration of handler \"foo\": "
      "got string, expected an object");

  input = R"JSON({
    "categories": { "folly": { "level": "ERR" } },
    "handlers": {
      "foo": {}
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "no handler type specified for log handler \"foo\"");

  input = R"JSON({
    "categories": { "folly": { "level": "ERR" } },
    "handlers": {
      "foo": {
        "type": 19
      }
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "unexpected data type for \"type\" field of handler \"foo\": "
      "got integer, expected a string");

  input = R"JSON({
    "categories": { "folly": { "level": "ERR" } },
    "handlers": {
      "foo": {
        "type": "custom",
        "options": true
      }
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "unexpected data type for \"options\" field of handler \"foo\": "
      "got boolean, expected an object");

  input = R"JSON({
    "categories": { "folly": { "level": "ERR" } },
    "handlers": {
      "foo": {
        "type": "custom",
        "options": ["foo", "bar"]
      }
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "unexpected data type for \"options\" field of handler \"foo\": "
      "got array, expected an object");

  input = R"JSON({
    "categories": { "folly": { "level": "ERR" } },
    "handlers": {
      "foo": {
        "type": "custom",
        "options": {"bar": 5}
      }
    }
  })JSON";
  EXPECT_THROW_RE(
      parseLogConfig(input),
      LogConfigParseError,
      "unexpected data type for option \"bar\" of handler \"foo\": "
      "got integer, expected a string");
}

TEST(LogConfig, toJson) {
  auto config = parseLogConfig("");
  auto expectedJson = folly::parseJson(R"JSON({
  "categories": {},
  "handlers": {}
})JSON");
  EXPECT_EQ(expectedJson, logConfigToDynamic(config));

  config = parseLogConfig(
      "ERROR:h1,foo.bar:=FATAL,folly=INFO:; "
      "h1=custom,foo=bar");
  expectedJson = folly::parseJson(R"JSON({
  "categories" : {
    "" : {
      "inherit" : true,
      "level" : "ERR",
      "handlers" : ["h1"]
    },
    "folly" : {
      "inherit" : true,
      "level" : "INFO",
      "handlers" : []
    },
    "foo.bar" : {
      "inherit" : false,
      "level" : "FATAL"
    }
  },
  "handlers" : {
    "h1": {
      "type": "custom",
      "options": { "foo": "bar" }
    }
  }
})JSON");
  EXPECT_EQ(expectedJson, logConfigToDynamic(config));
}
