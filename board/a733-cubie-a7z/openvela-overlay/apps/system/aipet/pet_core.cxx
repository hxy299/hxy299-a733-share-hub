/* SPDX-License-Identifier: Apache-2.0 */
#include "pet_core.hxx"
#include <regex>
#include <stdexcept>

namespace aipet
{
namespace
{
const char *allowed[] = {
  "motor.forward", "motor.backward", "motor.turn_left", "motor.turn_right",
  "motor.stop", "servo.rotate", "servo.nod", "servo.shake", "servo.wave",
  "led.on", "led.off", "led.rainbow"
};

bool whitespace(unsigned int cp)
{
  return (cp >= 9 && cp <= 13) || (cp >= 0x1c && cp <= 0x20) ||
         cp == 0x85 || cp == 0xa0 || cp == 0x1680 ||
         (cp >= 0x2000 && cp <= 0x200a) || cp == 0x2028 || cp == 0x2029 ||
         cp == 0x202f || cp == 0x205f || cp == 0x3000;
}

/* Python str.strip / re.sub(r'\s+', ' ', ...) over valid UTF-8. */
std::string normalized(const std::string &text, bool collapse)
{
  std::string out;
  std::string pending;
  for (std::size_t i = 0; i < text.size();)
    {
      const std::size_t begin = i;
      unsigned char byte = static_cast<unsigned char>(text[i++]);
      unsigned int cp = byte;
      unsigned int continuation = 0;
      unsigned int minimum = 0;
      if (byte >= 0xc2 && byte <= 0xdf)
        { cp = byte & 31; continuation = 1; minimum = 0x80; }
      else if (byte >= 0xe0 && byte <= 0xef)
        { cp = byte & 15; continuation = 2; minimum = 0x800; }
      else if (byte >= 0xf0 && byte <= 0xf4)
        { cp = byte & 7; continuation = 3; minimum = 0x10000; }
      else if (byte >= 0x80)
        throw std::invalid_argument("invalid UTF-8");
      for (unsigned int j = 0; j < continuation; j++)
        {
          if (i == text.size() ||
              (static_cast<unsigned char>(text[i]) & 0xc0) != 0x80)
            throw std::invalid_argument("invalid UTF-8");
          cp = (cp << 6) | (static_cast<unsigned char>(text[i++]) & 63);
        }
      if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        throw std::invalid_argument("invalid UTF-8");
      if (whitespace(cp))
        {
          if (!out.empty())
            {
              if (collapse) pending = " ";
              else pending.append(text, begin, i - begin);
            }
        }
      else
        {
          out += pending;
          pending.clear();
          out.append(text, begin, i - begin);
        }
    }
  return out;
}

bool match(const Rule &rule, const std::string &text)
{
  for (const auto &keyword : rule.keywords)
    if (text.find(keyword) != std::string::npos) return true;
  return false;
}
}

bool parse_reply(const std::string &raw, Reply &out)
{
  out = Reply{};
  if (raw.size() > 4096 || raw.find('\0') != std::string::npos) return false;
  try
    {
      normalized(raw, false);  /* Validate before exposing an action. */
      static const std::regex action(R"(\[ACTION:([a-z_.]+\([^)]*\))\])");
      static const std::regex emotion("\\[(开心|难过|惊讶|生气|思考|安慰|普通)\\]");
      Reply candidate;
      for (std::sregex_iterator it(raw.begin(), raw.end(), action), end;
           it != end; ++it)
        {
          const std::string call = (*it)[1].str();
          const std::string method = call.substr(0, call.find('('));
          for (const char *name : allowed)
            if (method == name)
              {
                if (candidate.actions.size() == 32 || call.size() > 160)
                  return false;  /* No partial action batch. */
                candidate.actions.push_back(call);
                break;
              }
        }
      const std::string action_clean = normalized(std::regex_replace(raw, action, ""), true);
      std::smatch found;
      if (std::regex_search(action_clean, found, emotion))
        candidate.emotion = found[1].str(); /* First in text, not list order. */
      candidate.text = normalized(std::regex_replace(action_clean, emotion, ""), true);
      out = std::move(candidate);
      return true;
    }
  catch (const std::exception &)
    {
      return false;
    }
}

Turn Conversation::run(const std::string &user)
{
  Turn turn;
  if (state_ != State::idle) return turn;
  state_ = State::routing;
  try
    {
      if (user.size() > 4096 || user.find('\0') != std::string::npos)
        throw std::invalid_argument("input limit");
      std::string text = normalized(user, false);
      for (char &c : text)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 'a' - 'A');
      std::string raw;
      bool decided = false;
      if (text.empty())
        {
          raw = "嗯？我没听清，再说一次吧~";
          decided = true;
        }
      for (const auto &rule : rules_.direct)
        if (!decided && match(rule, text))
          {
            if (!rule.replies.empty())
              raw = ports_.expand_template(rule.replies[ports_.choose(rule.replies.size()) % rule.replies.size()]);
            decided = true;
          }
      if (!decided)
        {
          turn.route = Route::cloud;
          for (const auto &rule : rules_.local)
            if (match(rule, text)) { turn.route = Route::local; break; }
          if (turn.route == Route::cloud && !ports_.online())
            turn.route = Route::local;
          state_ = State::generating;
          bool ok;
          if (turn.route == Route::cloud)
            {
              ok = ports_.cloud_reply(user, raw);
              if (!ok)
                {
                  turn.cloud_fallback = true;
                  turn.route = Route::local;
                  raw.clear();
                  ok = ports_.local_reply(user, raw);
                }
            }
          else ok = ports_.local_reply(user, raw);
          if (!ok) throw std::runtime_error("backend unavailable");
        }
      if (!parse_reply(raw, turn.reply)) throw std::runtime_error("reply rejected");
      state_ = State::presenting;
      turn.ok = ports_.present(turn.reply);
      if (turn.ok) completed_++;
      ports_.return_idle();
    }
  catch (const std::exception &)
    {
      state_ = State::error;
      turn.ok = false;
      /* Real adapters must provide a nonthrowing cleanup operation. */
      try { ports_.return_idle(); } catch (...) {}
    }
  state_ = State::idle;
  return turn;
}
}
