#ifndef RAPIDJSON_WREADER_H_
#define RAPIDJSON_WREADER_H_

/*! \file wreader.h */

#include "rapidjson.h"
#include "reader.h"
#include "allocators.h"
#include "internal/meta.h"
#include "internal/stack.h"
#include "internal/strtod.h"
#include <limits>

#include <stdio.h>

#ifdef __clang__
RAPIDJSON_DIAG_PUSH
RAPIDJSON_DIAG_OFF(old-style-cast)
RAPIDJSON_DIAG_OFF(padded)
RAPIDJSON_DIAG_OFF(switch-enum)
#endif

#ifdef __GNUC__
RAPIDJSON_DIAG_PUSH
RAPIDJSON_DIAG_OFF(effc++)
#endif

//!@cond RAPIDJSON_HIDDEN_FROM_DOXYGEN
#define RAPIDJSON_NOTHING /* deliberately empty */
#ifndef RAPIDJSON_PARSE_ERROR_EARLY_RETURN
#define RAPIDJSON_PARSE_ERROR_EARLY_RETURN(value) \
    RAPIDJSON_MULTILINEMACRO_BEGIN \
    if (RAPIDJSON_UNLIKELY(HasParseError())) { return value; } \
    RAPIDJSON_MULTILINEMACRO_END
#endif
#define RAPIDJSON_PARSE_ERROR_EARLY_RETURN_VOID \
    RAPIDJSON_PARSE_ERROR_EARLY_RETURN(RAPIDJSON_NOTHING)

#define RAPIDJSON_PARSE_ERROR_AT(parseErrorCode, p) \
    RAPIDJSON_MULTILINEMACRO_BEGIN \
    RAPIDJSON_PARSE_ERROR_NORETURN(parseErrorCode, offset_ + (p - start)); \
    RAPIDJSON_PARSE_ERROR_EARLY_RETURN(p); \
    RAPIDJSON_MULTILINEMACRO_END
//!@endcond

#define ABORT_WITH(v) \
  RAPIDJSON_MULTILINEMACRO_BEGIN \
  fprintf(stderr, #v "\n"); \
  fflush(stderr); \
  abort(); \
  RAPIDJSON_MULTILINEMACRO_END
#define CHECKEND(v) if ((v) > end) return SignalIncomplete(start);
#define SKIP_WHITESPACE(v) while (IsWhiteSpace(v)) CHECKEND(++v)
#define log(v) fprintf(stderr, "%s\n", v);

RAPIDJSON_NAMESPACE_BEGIN

static inline bool consume(const char** paddr, const char m) {
  const char* p = *paddr;
  if (*p == m) {
    // advance to next char
    *paddr = p + 1;
    return true;
  } else {
    return false;
  }
}

// {?function():boolean}
class CompletedIteration {
  public:
    bool virtual operator() () { return true; }

};
static CompletedIteration* Noop = new CompletedIteration();

template <typename SourceEncoding, typename TargetEncoding, typename StackAllocator = CrtAllocator>
class GenericWReader {
public:

    GenericWReader(
        StackAllocator* stackAllocator = 0,
        size_t stackCapacity = kDefaultStackCapacity,
        CompletedIteration* completedIteration = Noop)
      : stack_(stackAllocator, stackCapacity), parseResult_(), completedIteration_(completedIteration) {}

    //! Initialize JSON text token-by-token parsing
    /*!
     */
    void IterativeParseInit() {
        parseResult_.Clear();
        state_ = IterativeParsingStartState;
        offset_ = 0;
    }

    template<unsigned parseFlags, typename Handler>
    void Write(const char* chunk, size_t chunkSize, Handler& handler) {
      const char* end = chunk + (chunkSize - 1);
      const char* current = chunk;
      const char* next = chunk;
      while(next <= end) {
        current = next;
        next = ProcessNext(current, end, handler);
        offset_ += (next - current);
      }
      fprintf(stderr, "Processed up to offset %ld\n", offset_);
    }

    ParseResult IterativeParseFinish() {
      return parseResult_;
    }

    bool HasParseError() {
      return false;
    }

    ParseErrorCode GetParseErrorCode() {
      return ParseErrorCode::kParseErrorNone;;
    }

    size_t GetErrorOffset() {
      return 0;
    }

protected:
    void SetParseError(ParseErrorCode code, size_t offset) { parseResult_.Set(code, offset); }

private:
    template <typename Handler>
    const char* ProcessNext(const char* p, const char* end, Handler& handler) {
      // skip whitespaces
      if (IsWhiteSpace(p)) return p + 1;
      return ParseValue(p, end, handler);
    }

    template <typename Handler>
    const char* ParseValue(const char* p, const char* end, Handler& handler) {
      switch (*p) {
        case '{': return ParseObject(p, end, handler) + 1;
        case '[': return ParseArray(p, end, handler) + 1;
        case 't': return ParseTrue(p, end, handler) + 1;
        case 'f': return ParseFalse(p, end, handler) + 1;
        case 'n': return ParseNull(p, end, handler) + 1;
        //case '"': ParseString<parseFlags>(is, handler); break;
        default: return ParseNumber(p, end, handler) + 1;
      }
    }

    inline bool IsWhiteSpace(const char *p) {
      return (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t');
    }

    const char* SignalIncomplete(const char* restartHere) {
      // TODO: signal that we couldn't finish parsing and need to reuse the remaining
      // part of the chunk as prefix when we get the next chunk written to us
      return restartHere;
    }


    template <typename Handler>
    const char* ParseObject(const char* start, const char* end, Handler& handler) {
      const char* p = start;
      RAPIDJSON_ASSERT(*p == '{');
      log("object open")
      CHECKEND(++p)

      if (RAPIDJSON_UNLIKELY(!handler.StartObject()))
        RAPIDJSON_PARSE_ERROR_AT(kParseErrorTermination, p);

      RAPIDJSON_PARSE_ERROR_EARLY_RETURN(p);

      // Skip white space after '{' to get to first property
      SKIP_WHITESPACE(p)

      // empty object
      if (*p == '}') {
        log("object close")
        if (RAPIDJSON_UNLIKELY(!handler.EndObject(0)))
          RAPIDJSON_PARSE_ERROR_AT(kParseErrorTermination, p);
        return p + 1;
      }

      return p + 1;
    }

    template <typename Handler>
    const char* ParseArray(const char* start, const char* end, Handler& handler) {
      const char* p = start;
      RAPIDJSON_ASSERT(*p == '[');
      log("array open")
      CHECKEND(++p)

      if (RAPIDJSON_UNLIKELY(!handler.StartArray()))
        RAPIDJSON_PARSE_ERROR_AT(kParseErrorTermination, p);

      RAPIDJSON_PARSE_ERROR_EARLY_RETURN(p);

      // Skip white space after '[' to get to first element
      SKIP_WHITESPACE(p)

      // empty array
      if (*p == ']') {
        log("array close")
        if (RAPIDJSON_UNLIKELY(!handler.EndArray(0)))
          RAPIDJSON_PARSE_ERROR_AT(kParseErrorTermination, p);
        return p + 1;
      }

      for (SizeType elementCount = 0;;) {
        p = ParseValue(p, end, handler);
        RAPIDJSON_PARSE_ERROR_EARLY_RETURN(p);

        ++elementCount;
        SKIP_WHITESPACE(p)
        RAPIDJSON_PARSE_ERROR_EARLY_RETURN(p);

        if (*p == ',') {
          SKIP_WHITESPACE(++p)
          RAPIDJSON_PARSE_ERROR_EARLY_RETURN(p);
          return p + 1;
        }
        else if (*p == ']') {
          log("array close")
          if (RAPIDJSON_UNLIKELY(!handler.EndArray(elementCount)))
            RAPIDJSON_PARSE_ERROR_AT(kParseErrorTermination, p);
          return p + 1;
        }
        else
          RAPIDJSON_PARSE_ERROR_AT(kParseErrorArrayMissCommaOrSquareBracket, p);
      }

      return p + 1;
    }


    template <typename Handler>
    const char* ParseTrue(const char* start, const char* end, Handler& handler) {
      const char* p = start;
      RAPIDJSON_ASSERT(*p == 't');
      // true has 4 chars
      CHECKEND(p + 4)

      if (RAPIDJSON_LIKELY(p[1] == 'r' && p[2] == 'u' && p[3] == 'e')) {
        log("true")
        if (RAPIDJSON_UNLIKELY(!handler.Bool(true)))
          RAPIDJSON_PARSE_ERROR_AT(kParseErrorTermination, p);
        return p + 4;
      }
      else
          RAPIDJSON_PARSE_ERROR_AT(kParseErrorValueInvalid, p);

      ABORT_WITH("Unexpected exception when parsing \"true\".");
    }

    template <typename Handler>
    const char* ParseFalse(const char* start, const char* end, Handler& handler) {
      const char* p = start;
      RAPIDJSON_ASSERT(*p == 'f');
      // false has 5 chars
      CHECKEND(p + 5)

      if (RAPIDJSON_LIKELY(p[1] == 'a' && p[2] == 'l' && p[3] == 's' && p[4] == 'e')) {
        log("false")
        if (RAPIDJSON_UNLIKELY(!handler.Bool(false)))
          RAPIDJSON_PARSE_ERROR_AT(kParseErrorTermination, p);
        return p + 5;
      }
      else
          RAPIDJSON_PARSE_ERROR_AT(kParseErrorValueInvalid, p);

      ABORT_WITH("Unexpected exception when parsing \"false\".");
    }

    template <typename Handler>
    const char* ParseNull(const char* start, const char* end, Handler& handler) {
      const char* p = start;
      RAPIDJSON_ASSERT(*p == 'n');
      // false has 4 chars
      CHECKEND(p + 4)

      if (RAPIDJSON_LIKELY(p[1] == 'u' && p[2] == 'l' && p[3] == 'l')) {
        log("null")
        if (RAPIDJSON_UNLIKELY(!handler.Null()))
          RAPIDJSON_PARSE_ERROR_AT(kParseErrorTermination, p);
        return p + 4;
      }
      else
          RAPIDJSON_PARSE_ERROR_AT(kParseErrorValueInvalid, p);

      ABORT_WITH("Unexpected exception when parsing \"null\".");
    }

    template <typename Handler>
    const char* ParseNumber(const char* start, const char* end, Handler& handler) {
      const char* p = start;
      bool minus = consume(&p, '-');
      return p + 1;
    }

//
// START copied from reader.h and should most likely reuse
//

    // Iterative Parsing

    // States
    enum IterativeParsingState {
        IterativeParsingFinishState = 0, // sink states at top
        IterativeParsingErrorState,      // sink states at top
        IterativeParsingStartState,

        // Object states
        IterativeParsingObjectInitialState,
        IterativeParsingMemberKeyState,
        IterativeParsingMemberValueState,
        IterativeParsingObjectFinishState,

        // Array states
        IterativeParsingArrayInitialState,
        IterativeParsingElementState,
        IterativeParsingArrayFinishState,

        // Single value state
        IterativeParsingValueState,

        // Delimiter states (at bottom)
        IterativeParsingElementDelimiterState,
        IterativeParsingMemberDelimiterState,
        IterativeParsingKeyValueDelimiterState,

        cIterativeParsingStateCount
    };

//
// END copied from reader.h
//

    static const size_t kDefaultStackCapacity = 256;    //!< Default stack capacity in bytes for storing a single decoded string.
    internal::Stack<StackAllocator> stack_;  //!< A stack for storing decoded string temporarily during non-destructive parsing.
    ParseResult parseResult_;
    IterativeParsingState state_;
    CompletedIteration* completedIteration_;
    size_t offset_;
}; // class GenericWReader

//! WReader with UTF8 encoding and default allocator.
typedef GenericWReader<UTF8<>, UTF8<> > WReader;

RAPIDJSON_NAMESPACE_END


#ifdef __clang__
RAPIDJSON_DIAG_POP
#endif


#ifdef __GNUC__
RAPIDJSON_DIAG_POP
#endif

#ifdef _MSC_VER
RAPIDJSON_DIAG_POP
#endif

#endif // RAPIDJSON_WREADER_H_
