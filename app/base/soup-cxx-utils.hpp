/*
 * soup-cxx-utils.hpp
 *
 * Copyright (C) 2017 - seagetch
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SOUP_CXX_UTILS_HPP__
#define __SOUP_CXX_UTILS_HPP__

extern "C" {
#include <libsoup/soup.h>
#include <string.h>
};
#include "delegators.hpp"
#include "glib-cxx-utils.hpp"

__DECLARE_GTK_CAST__(SoupServer, SOUP_SERVER, soup_server);
__DECLARE_GTK_CAST__(SoupMessage, SOUP_MESSAGE, soup_message);

namespace Soup {

class ISoupSession : public GLib::IObject<SoupSession> {
  using super = GLib::IObject<SoupSession>;
public:
  using Handler = Delegators::Delegator<void(SoupSession*, SoupMessage*)>;

  class Defferred {
    using Handler = Delegator<SoupMessage*(SoupMessage*)>;
    CXXPointer<Handler> handler;
    Defferred* prev;
    Defferred* next;
    int refCount;
  public:
    Defferred() : handler(NULL), next(NULL), prev(NULL), refCount(0) {};
    Defferred(Handler* handler) : handler(handler), next(NULL), prev(NULL), refCount(0) {};
    Defferred* then(Handler* handler) {
      next = new Defferred(handler);
      next->prev = this;
      incRef();
      return next;
    }
    void incRef() {
      refCount ++;
    }
    void decRef() {
      refCount --;
      if (prev)
        prev->decRef();
      prev = NULL;
      if (refCount <= 0) {
        delete this;
      }
    }

    static
    void callback(SoupSession* session, SoupMessage* message, gpointer user_data) {
      ISoupSession i_session(session);
      Defferred* defferred = reinterpret_cast<Defferred*>(user_data);

      while (defferred && !defferred->handler) {
        if (defferred->next)
          defferred = defferred->next;
      }
      if (!defferred)
        return;

      SoupMessage* new_message = (*(defferred->handler)) (message);
      Defferred* next = defferred->next;
      if (new_message)
        i_session.queue_message(new_message, next);
      else {
        defferred->decRef();
      }
    };
  };

  ISoupSession() : super() { };
  ISoupSession(SoupSession* session) : super(session) { };
  ISoupSession(const ISoupSession& session) : super(session.obj) { };

  Defferred* queue_message(SoupMessage *msg, Defferred* defferred = NULL) {
    if (!defferred)
      defferred = new Defferred();
    SoupSession* session = SOUP_SESSION(obj);
    soup_session_queue_message (session, msg, Defferred::callback, defferred);
    return defferred;
  }

  void abort() {
    SoupSession* session = SOUP_SESSION(obj);
    soup_session_abort (session);
  }

  void cancel(SoupMessage* msg, guint status) {
    SoupSession* session = SOUP_SESSION(obj);
    soup_session_cancel_message (session, msg, status);
  }

};


class ISoupServer : public GLib::IObject<SoupServer> {
  using super = GLib::IObject<SoupServer>;
public:
  using Handler = Delegators::Delegator<void(SoupServer*, SoupMessage*, const char*, GHashTable*, SoupClientContext*)>;

  ISoupServer() : super () { };
  ISoupServer(SoupServer* server) : super(server) { };
  ISoupServer(const ISoupServer& server) : super(server.obj) { };
  
  void add_handler(const char *path,
                   Handler* callback) {
    SoupServer* server = SOUP_SERVER(obj);
    soup_server_add_handler (server, path, 
      Handler::callback, callback,
      Delegators::destroy_cxx_object_callback<Handler>);
  }
  
  void add_early_handler(const char *path,
                         Handler* callback) {
    SoupServer* server = SOUP_SERVER(obj);
    soup_server_add_early_handler (server, path, 
      Handler::callback, callback,
      Delegators::destroy_cxx_object_callback<Handler>);
  }
  
  void remove_handler(const char *path) {
    SoupServer* server = SOUP_SERVER(obj);
    soup_server_remove_handler (server, path);
  }
  
};

inline auto ref(SoupServer* server) {
  return ISoupServer(server);
}


template<typename Ret, typename... Args>
class Router {

public:

  class Matched {
    GLib::StringList path;
    GLib::HashTable _data;
  public:
    Matched() : path(NULL), _data(g_hash_table_new(g_str_hash, g_str_equal)) { }
    Matched(const Matched& src) : path(g_strdupv(src.path.ptr())), _data(g_hash_table_ref(src._data)) { }
    ~Matched() { }

    auto data() { return GLib::ref<const gchar*, gchar*>(_data); }
    gchar* const* get_path() const { return path; }
    void set_path(gchar** path) {
      if (path) {
        this->path = g_strdupv(path); 
      } else {
        this->path = (gchar**)NULL;
      }
    }
  };

  typedef bool rule_handler(Matched*);
  typedef Delegators::Delegator<Ret(Matched*, Args...)> rule_delegator;

protected:

  struct Rule {

    struct TokenRule {
      virtual bool loop() { return false; }
      virtual bool test(const gchar* token, Matched& result) = 0;
    };

    struct Match: TokenRule {
      GLib::CString token;

      Match(const gchar* name) : token(g_strdup(name)) {}
      Match(const Match& src) : token(g_strdup(src.token)) {}
      virtual bool test(const gchar* tested_token, Matched& result) {
        return strcmp(token, tested_token) == 0;
      }
    };

    struct Name: TokenRule {
      GLib::CString name;

      Name(const gchar* n) : name(g_strdup(n)) {}
      Name(const Name& src) : name(g_strdup(src.name)) {}
      virtual bool test(const gchar* tested_token, Matched& result) {
        result.data().insert(name, g_strdup(tested_token));
        return true;
      }
    };

    struct Select: TokenRule {
      GLib::CString    name;
      GLib::StringList candidates;

      Select(const gchar* n, gchar** c) : name(g_strdup(n)), candidates(g_strdupv(c)) {}
      Select(const Select& src) : name(g_strdup(src.name)), candidates(g_strdupv(src.candidates)) {}
      ~Select() { }

      virtual bool test(const gchar* tested_token, Matched& result) {
        for (auto next_token : candidates) {
          if (strcmp(next_token, tested_token) == 0) {
            result.data().insert(name, g_strdup(tested_token));
            return true;
          }
        }
        return false;
      }
    };

    struct Any: TokenRule {
      ~Any() { };
      virtual bool test(const gchar* tested_token, Matched& result) {
        for (int i = 0; true; i ++) {
          GLib::CString any=g_strdup_printf("*%d", i);
          if (!result.data().lookup(any)) {
            result.data().insert("*", g_strdup(tested_token));
            break;
          }
        }
        return true;
      }
    };

    struct AnyLoop: TokenRule {
      ~AnyLoop() { };
      virtual bool loop() { return true; }
      virtual bool test(const gchar* tested_token, Matched& result) {
        gchar* path = result.data().lookup("**");
        if (!path) {
          result.data().insert("**", g_strdup(tested_token));
        } else {
          result.data().insert("**", g_strdup_printf("%s/%s", path, tested_token));
          g_free(path);
        }
        return true;
      }
    };

    GLib::Array _rules;
    CXXPointer<rule_delegator> handler;

    auto rules() { return GLib::IArray<TokenRule*>(_rules); }

    Rule(const gchar* path, rule_delegator* h) {
      _rules  = g_array_new(TRUE, TRUE, sizeof(TokenRule*));
      handler = h;
      g_print("Rule::constructor parse %s\n", path);
      parse_rule(path);
    }

    void parse_rule(const gchar* path) {
      GLib::StringList rule_path(g_strsplit(path, "/", 256));

      for (auto token : rule_path) {

        if (!token || strlen(token) == 0)
          continue;

        g_print("Rule::constructor parse: transform %s\n", token);

        switch (token[0]) {
        case '{': {
          gchar* word_term = strstr(&(token[1]),"}");
          if (word_term) {
            GLib::CString varname = strndup(&(token[1]), word_term - &(token[1]));

            gchar* type_term      = strstr(varname, ":");
            if (type_term) { // Have type declaration or selection
              GLib::CString type          = strndup(varname, type_term - varname.ptr());
              GLib::CString subname       = strdup(type_term + 1);
              GLib::StringList candidates = g_strsplit(type, "|", 256);

              g_print("register Select(%s)\n", subname.ptr());
              rules().append(new Select(subname, candidates));

            } else { // Have no type declaration, assume string type.
              g_print("register Name(%s)\n", (const gchar*)varname);
              rules().append(new Name(varname));

            }
          }
          break;
        }
        case '*': {
          if(token[1] == '*') {
            g_print("register any path\n");
            rules().append(new AnyLoop());
          } else {
            g_print("register any\n");
            rules().append(new Any());
          }
          break;
        }
        default:
          rules().append(new Match(token));
          g_print("register Match(%s)\n", token);
          break;
        }

      };
    };

    ~Rule() {
      for (auto i : rules()) { delete i; }
    }

    Matched test(gchar* const* uri, const gchar* data) {
      Matched result;
      if (rules().size() == 0)
        return result;

      gchar** tested_token = (gchar**)uri;
      for (auto i: rules()) {
        do {
          const gchar* token = *tested_token;
          while (token && strlen(token) == 0)
            token = *(++tested_token);
          if (!token) { // Tested URI is short.
  //          g_print("tested URI is too short.\n");
            if (i->loop()) // i is infinite loop rule. exit from loop.
              break;
            return result;
          }
          if (!i->test(token, result)) {
  //          g_print("Not matched.\n");
            return result;
          }
          tested_token ++;
        } while (i->loop());
      }
      while (*tested_token && strlen(*tested_token) == 0)
        ++tested_token;
      if (*tested_token) {
//        g_print("tested URI is too long. remained '%s'\n", *tested_token);
        return result;
      }
      result.set_path((gchar**)uri);
      return result;
    }

  };

  GLib::Array _rules;
  auto rules() { return GLib::IArray<Router::Rule*>(_rules); }


public:

  Router() {
    _rules = g_array_new(TRUE, TRUE, sizeof(Router::Rule*));
  }

  ~Router() {
    for (auto i: rules()) { delete i; }
  }

  Router& route(const gchar* route, rule_delegator* handler) {
    rules().append(new Rule(route, handler));
    return *this;
  };

  Ret dispatch(const gchar* uri, const gchar* data, Args... args) {
    GLib::CString decoded_uri = soup_uri_decode (uri);
    GLib::StringList tested_path = g_strsplit(decoded_uri, "/", 256);

    for ( auto i: rules() ) {
      Matched matched = i->test(tested_path.ptr(), data);
      gchar* const* path = matched.get_path();
      if (matched.get_path() && i->handler) {
        return (*(i->handler))(&matched, args...);
      }
    }
    return Ret();
  }


}; // Class Router


}; // namespace Soup

#endif /* __SOUP_CXX_UTILS_HPP__ */
