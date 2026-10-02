/* SPDX-License-Identifier: GPL-3.0-or-later */
/* First-run Painter profile migration. Parsing is deliberately independent of
 * registered GTypes: user-install runs before tool/resource initialization.
 * We edit token spans, never arbitrary substrings, and archive every input
 * before publishing a converted file. Unknown data stays in both the converted
 * file and the immutable, content-addressed original even if a GimpConfig loader
 * does not know how to reserialize it yet. */
#include "gimppainterprofile.h"
#include "painter/gio-type-traits.hpp"
#include "painter/resources.hpp"
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>
#include <cerrno>
#include <cstring>
namespace {
struct Node {
  enum Kind { list, atom, string } kind;
  std::size_t begin, end;
  std::string value;
  std::vector<Node> children;
};
class Parser {
  const std::string& input;
  std::size_t pos = 0, count = 0;
  void space () {
    while (pos < input.size ()) {
      if (g_ascii_isspace (input[pos])) { ++pos; continue; }
      if (input[pos] == '#' || input[pos] == ';') {
        while (pos < input.size () && input[pos] != '\n') ++pos;
        continue;
      }
      break;
    }
  }
  Node node (unsigned depth) {
    if (depth > 128 || ++count > 1000000) throw std::runtime_error ("Profile nesting or token limit exceeded");
    space (); if (pos == input.size ()) throw std::runtime_error ("Unexpected end of profile");
    Node result {Node::atom, pos, pos, {}, {}};
    if (input[pos] == '(') {
      result.kind = Node::list; ++pos; space ();
      while (pos < input.size () && input[pos] != ')') { result.children.push_back (node (depth + 1)); space (); }
      if (pos == input.size ()) throw std::runtime_error ("Unterminated profile form");
      ++pos;
    } else if (input[pos] == '"') {
      result.kind = Node::string; ++pos;
      while (pos < input.size () && input[pos] != '"') {
        char c = input[pos++];
        if (c == '\\') {
          if (pos == input.size ()) throw std::runtime_error ("Unterminated profile escape");
          c = input[pos++];
          switch (c) { case 'n': c='\n'; break; case 'r': c='\r'; break; case 't': c='\t'; break;
            case 'b': c='\b'; break; case 'f': c='\f'; break;
            default: if (c >= '0' && c <= '7') {
              unsigned value=c-'0', n=1;
              while(n<3 && pos<input.size() && input[pos]>='0' && input[pos]<='7') { value=value*8+input[pos++]-'0'; ++n; }
              c=static_cast<char>(value);
            }
          }
        }
        result.value += c;
      }
      if (pos == input.size ()) throw std::runtime_error ("Unterminated profile string");
      ++pos;
    } else {
      if (input[pos] == ')') throw std::runtime_error ("Unexpected closing profile form");
      while (pos < input.size () && !g_ascii_isspace (input[pos]) && input[pos]!='(' && input[pos]!=')' && input[pos]!='#' && input[pos]!=';') result.value += input[pos++];
      if (result.value.empty ()) throw std::runtime_error ("Invalid profile token");
    }
    result.end=pos; return result;
  }
public:
  explicit Parser (const std::string& text): input(text) {
    if(input.size()>32*1024*1024 || input.find('\0')!=std::string::npos) throw std::runtime_error("Invalid or oversized profile");
  }
  std::vector<Node> parse () {
    std::vector<Node> nodes; space();
    while(pos<input.size()) { auto next=node(0); if(next.kind!=Node::list) throw std::runtime_error("Expected profile form"); nodes.push_back(std::move(next)); space(); }
    return nodes;
  }
};
std::string key(const Node&n) { return n.kind==Node::list && !n.children.empty() && n.children[0].kind==Node::atom?n.children[0].value:""; }
const Node* argument(const Node&n) { return n.children.size()>1?&n.children[1]:nullptr; }
std::string quoted(const std::string&s) { GimpPainter::String escaped(g_strescape(s.c_str(),nullptr)); return "\""+std::string(escaped.get())+"\""; }
struct Edit { std::size_t begin,end; std::string text; };
void edit(std::vector<Edit>&edits,const Node&n,const std::string&value) {
  if(n.value!=value) edits.push_back({n.begin,n.end,n.kind==Node::string?quoted(value):value});
}
std::string tool(const std::string&value) {
  if(value=="gimp-mypaint-tool") return "gimp-painter-mypaint-tool";
  if(value=="gimp-smudge-tool") return "gimp-painter-smudge-tool";
  if(value=="gimp-blend-tool") return "gimp-gradient-tool";
  return value;
}
std::string option_type(const std::string&value) {
  if(value=="GimpMypaintOptions") return "GimpPainterMybrushOptions";
  if(value=="GimpSmudgeOptions") return "GimpPainterSmudgeOptions";
  if(value=="GimpBucketFillBrushOptions") return "GimpFillBrushOptions";
  if(value=="GimpBlendOptions") return "GimpGradientOptions";
  if(value=="GimpImageMapOptions") return "GimpFilterOptions";
  return value;
}
std::string paint_mode (const std::string& value) {
  static const char *old_names[] = {
    "normal-mode", "dissolve-mode", "behind-mode", "multiply-mode", "screen-mode",
    "overlay-mode", "difference-mode", "addition-mode", "subtract-mode", "darken-only-mode",
    "lighten-only-mode", "hue-mode", "saturation-mode", "color-mode", "value-mode",
    "divide-mode", "dodge-mode", "burn-mode", "hardlight-mode", "softlight-mode",
    "grain-extract-mode", "grain-merge-mode", "color-erase-mode", "erase-mode", "replace-mode",
    "anti-erase-mode", "src-in-mode", "dst-in-mode", "src-out-mode", "dst-out-mode"
  };
  static const char *new_names[] = {
    "painter-normal", "dissolve", "behind-legacy", "painter-multiply", "screen-legacy",
    "overlay-legacy", "difference-legacy", "addition-legacy", "subtract-legacy", "darken-only-legacy",
    "lighten-only-legacy", "hsv-hue-legacy", "hsv-saturation-legacy", "hsl-color-legacy", "hsv-value-legacy",
    "divide-legacy", "dodge-legacy", "burn-legacy", "hardlight-legacy", "softlight-legacy",
    "grain-extract-legacy", "grain-merge-legacy", "color-erase-legacy", "painter-erase", "painter-replace",
    "painter-anti-erase", "painter-src-in", "painter-dst-in", "painter-src-out", "painter-dst-out"
  };
  for (unsigned i=0;i<G_N_ELEMENTS(old_names);++i)
    if (value==old_names[i]) return new_names[i];
  return value;
}
bool context_container(const std::string&k) {
  return k=="GimpToolInfo" || k=="GimpToolGroup" || k=="children" || k=="tool-options" || k=="GimpDeviceInfo";
}
bool proof(const std::vector<Node>&nodes) {
  for(const auto&n:nodes) {
    const auto k=key(n);const auto*a=argument(n);
    if(a && (k=="tool" || k=="GimpToolInfo" || k=="active-tool") &&
       a->kind==Node::string && a->value=="gimp-mypaint-tool")return true;
    // Fill retains its identifier in the modern port. Only its legacy stock-id
    // schema proves the old generation; a modern Fill tool is not evidence for
    // remapping independent standard Smudge settings.
    if(k=="GimpToolInfo" && a && a->kind==Node::string && a->value=="gimp-bucket-fill-brush-tool")
      for(const auto&field:n.children)if(key(field)=="stock-id")return true;
    if(k=="tool-options" && a && a->kind==Node::string &&
       (a->value=="GimpMypaintOptions" || a->value=="GimpBucketFillBrushOptions"))return true;
    if(context_container(k) && proof(n.children)) return true;
  }
  return false;
}
bool old_brush_tool(const std::string&name) {
  for(const char*n:{"gimp-paintbrush-tool","gimp-pencil-tool","gimp-airbrush-tool","gimp-eraser-tool","gimp-clone-tool","gimp-heal-tool","gimp-perspective-clone-tool","gimp-convolve-tool","gimp-smudge-tool","gimp-dodge-burn-tool","gimp-bucket-fill-brush-tool"})
    if(name==n)return true;
  return false;
}
bool contains_property(const std::vector<Node>&nodes,const std::string&name) {
  for(const auto&n:nodes)if(key(n)==name)return true;
  return false;
}
bool explicit_spacing(const std::vector<Node>&nodes) {
  for(const auto&n:nodes)if(key(n)=="brush-spacing" || key(n)=="brush-link-spacing" || key(n)=="painter-legacy-native-spacing")return true;
  return false;
}
bool explicit_hardness(const std::vector<Node>&nodes) {
  for(const auto&n:nodes)if(key(n)=="brush-hardness" || key(n)=="brush-link-hardness" || key(n)=="painter-legacy-native-hardness")return true;
  return false;
}
bool old_brush_options(const std::string&name) {
  for(const char*n:{"GimpPaintOptions","GimpAirbrushOptions","GimpEraserOptions","GimpCloneOptions","GimpHealOptions","GimpPerspectiveCloneOptions","GimpConvolveOptions","GimpSmudgeOptions","GimpDodgeBurnOptions","GimpBucketFillBrushOptions"})
    if(name==n)return true;
  return false;
}
bool tool_identifier(const std::string&name) {
  if(!g_str_has_prefix(name.c_str(),"gimp-") || !g_str_has_suffix(name.c_str(),"-tool"))return false;
  for(unsigned char c:name)if(!g_ascii_isalnum(c) && c!='-' && c!='_')return false;
  return true;
}
void collect_brush_tools(const std::vector<Node>&nodes,std::vector<std::string>&names) {
  for(const auto&n:nodes) {
    const auto*a=argument(n);
    if(key(n)=="GimpToolInfo" && a && a->kind==Node::string && tool_identifier(a->value))names.push_back(a->value);
    if(key(n)=="GimpToolGroup" || key(n)=="children")collect_brush_tools(n.children,names);
  }
}
std::string transform(const std::string&input,const std::string&kind,bool origin);
bool device_context_field(const std::string&k) {
  for(const char*name:{"tool","paint-info","foreground","background","opacity","paint-mode","brush","dynamics","pattern","gradient","palette","font","mypaint-brush"})
    if(k==name)return true;
  return false;
}
void convert(const std::vector<Node>&nodes,const std::string&kind,std::vector<Edit>&edits,const std::string&input) {
  bool painter_tool=false;
  for(const auto&n:nodes) {
    const auto*a=argument(n);
    if(key(n)=="tool" && a && a->kind==Node::string && a->value=="gimp-mypaint-tool")painter_tool=true;
  }
  for(const auto&n:nodes) {
    const auto k=key(n);const auto*a=argument(n);
    if(!a) continue;
    if(k=="GimpDeviceInfo") {
      bool nested=false; for(const auto&field:n.children) if(key(field)=="tool-options")nested=true;
      if(!nested) {
        // Old devices were contexts, not tool presets. Use the base options
        // type: inventing a full tool-specific options object would reset
        // settings the old device never stored when switching devices.
        std::string context="(tool-options \"GimpPainterDeviceOptions\"\n";
        bool found=false,opacity=false,palette=false,font=false,painter=false;
        for(const auto&field:n.children) {
          const auto name=key(field);
          if(device_context_field(name)) {
            found=true;context+=input.substr(field.begin,field.end-field.begin)+"\n";
            edits.push_back({field.begin,field.end,""});
            opacity|=name=="opacity"||name=="paint-mode";palette|=name=="palette";font|=name=="font";painter|=name=="mypaint-brush";
          } else if(field.kind==Node::list) convert({field},kind,edits,input);
        }
        if(found) {
          context+=")\n(use-fg-bg yes)\n(use-brush yes)\n(use-dynamics yes)\n(use-pattern yes)\n(use-gradient yes)\n(use-mypaint-brush no)\n";
          context+=std::string("(use-opacity-paint-mode ")+(opacity?"yes":"no")+")\n";
          context+=std::string("(use-palette ")+(palette?"yes":"no")+")\n";
          context+=std::string("(use-font ")+(font?"yes":"no")+")\n";
          context+=std::string("(use-painter-mypaint-brush ")+(painter?"yes":"no")+")\n";
          edits.push_back({n.end-1,n.end-1,transform(context,"device-options",true)});
        }
        continue;
      }
    }
    if(kind=="gimprc" && (k=="theme" || k=="show-tooltips" || k=="filter-tool-show-color-options")) {
      // These are the upstream first-run migration's explicitly unsupported
      // GTK2/theme/gamma-UI choices. The complete form remains in the archive.
      edits.push_back({n.begin,n.end,""});
    }
    else if(kind=="gimprc" && k=="style" && a->kind==Node::atom && a->value=="solid")edit(edits,*a,"fg-color");
    else if(kind=="gimprc" && k=="precision" && a->kind==Node::atom && g_str_has_suffix(a->value.c_str(),"-gamma"))
      edit(edits,*a,a->value.substr(0,a->value.size()-6)+"-non-linear");
    else if(kind=="gimprc" && k=="default-mypaint-brush") edit(edits,n.children[0],"default-painter-mypaint-brush");
    else if(kind=="gimprc" && k=="global-mypaint-brush") edit(edits,n.children[0],"global-painter-mypaint-brush");
    else if((k=="tool" || k=="active-tool" || k=="GimpToolInfo") && a->kind==Node::string) edit(edits,*a,tool(a->value));
    else if(k=="tool-options" && a->kind==Node::string) {
      edit(edits,*a,option_type(a->value));
      std::string defaults;
      if(a->value!="GimpPainterDeviceOptions" && !contains_property(n.children,"paint-mode"))
        defaults+="\n(paint-mode painter-normal)";
      if(old_brush_options(a->value) && a->value!="GimpSmudgeOptions" &&
         !contains_property(n.children,"painter-legacy-brush-geometry"))
        edits.push_back({a->end,a->end,"\n(painter-legacy-brush-geometry yes)"});
      if(old_brush_options(a->value) && !explicit_spacing(n.children))
        defaults+="\n(painter-legacy-native-spacing yes)";
      if(old_brush_options(a->value) && a->value!="GimpSmudgeOptions" && !explicit_hardness(n.children))
        defaults+="\n(painter-legacy-native-hardness yes)";
      if(!defaults.empty())edits.push_back({n.end-1,n.end-1,defaults});
    }
    else if(k=="paint-info" && a->kind==Node::string) {
      if(a->value=="gimp-mypaint" || (painter_tool && a->value=="gimp-paintbrush")) edit(edits,*a,"gimp-painter-mypaint");
      else if(a->value=="gimp-smudge") edit(edits,*a,"gimp-painter-smudge");
    } else if(k=="paint-mode" && a->kind==Node::atom) edit(edits,*a,paint_mode(a->value));
    else if(k=="mypaint-brush") edit(edits,n.children[0],"painter-mybrush");
    else if(k=="stock-id") {
      edit(edits,n.children[0],"icon-name");
      if(a->kind==Node::string && a->value=="gimp-tool-mypaint") edit(edits,*a,"gimp-tool-mypaint-brush");
      else if(a->kind==Node::string && a->value=="gimp-tool-blend") edit(edits,*a,"gimp-tool-gradient");
    } else if(k=="file-version" && kind=="toolrc" && a->kind==Node::atom && a->value=="1") edit(edits,*a,std::to_string(GIMP_PAINTER_MIGRATED_TOOLRC_VERSION));
    else if(k=="dynamics" && a->kind==Node::string && a->value=="Dynamics Off") edits.push_back({n.begin,n.end,"(dynamics-enabled no)"});
    /* Removed and not safely equivalent fields are intentionally not discarded.
     * Their original form is retained even when the modern loader ignores it. */
    if(context_container(k) || (kind=="gimprc" && (k=="default-image" || k=="default-grid")))
      convert(n.children,kind,edits,input);
  }
}
std::string transform(const std::string&input,const std::string&kind,bool origin) {
  const auto nodes=Parser(input).parse();
  if(!origin) return input;
  std::vector<Edit> edits; convert(nodes,kind,edits,input);
  std::sort(edits.begin(),edits.end(),[](const Edit&a,const Edit&b){return a.begin>b.begin;});
  std::string output=input; std::size_t limit=input.size();
  for(const auto&e:edits) { if(e.end>limit) throw std::runtime_error("Overlapping profile transformation"); output.replace(e.begin,e.end-e.begin,e.text); limit=e.begin; }
  if((kind=="contextrc" || kind.compare(0,13,"tool-options/")==0) && !contains_property(nodes,"paint-mode"))
    output+="\n(paint-mode painter-normal)\n";
  if(kind.compare(0,13,"tool-options/")==0 && old_brush_tool(kind.substr(13))) {
    if(kind.substr(13)!="gimp-smudge-tool" && !contains_property(nodes,"painter-legacy-brush-geometry"))
      output="(painter-legacy-brush-geometry yes)\n"+output;
    if(!explicit_spacing(nodes))output+="\n(painter-legacy-native-spacing yes)\n";
    if(kind.substr(13)!="gimp-smudge-tool" && !explicit_hardness(nodes))
      output+="\n(painter-legacy-native-hardness yes)\n";
  }
  // The pinned Painter overlay had no preference: it was always shown.
  // Materialize that proven behavior in a new native preference, never infer
  // a source key and never override an explicit imported value.
  if(kind=="gimprc" && !contains_property(nodes,"painter-canvas-ui"))
    output+="\n(painter-canvas-ui yes)\n";
  return output;
}
using File=GimpPainter::ObjectRef<GFile>;
using Info=GimpPainter::ObjectRef<GFileInfo>;
using Enumerator=GimpPainter::ObjectRef<GFileEnumerator>;
std::string failure(const char*operation,GError*error) { std::string result=std::string(operation)+": "+(error?error->message:"unknown error"); g_clear_error(&error); return result; }
File file(const std::string&path) { return File::adopt(g_file_new_for_path(path.c_str())); }
std::string join(const std::string&a,const std::string&b) { GimpPainter::String path(g_build_filename(a.c_str(),b.c_str(),nullptr)); return path.get(); }
std::string read(const std::string&path) {
  auto f=file(path); GError*error=nullptr;
  auto info=Info::adopt(g_file_query_info(f.get(),G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_SIZE,G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,nullptr,&error));
  if(!info) throw std::runtime_error(failure("Read profile metadata",error));
  if(g_file_info_get_file_type(info.get())!=G_FILE_TYPE_REGULAR || g_file_info_get_size(info.get())>32*1024*1024) throw std::runtime_error("Profile source is not a regular bounded file");
  gchar*data=nullptr;gsize length=0;
  if(!g_file_load_contents(f.get(),nullptr,&data,&length,nullptr,&error)) throw std::runtime_error(failure("Read profile",error));
  GimpPainter::String contents(data);return std::string(contents.get(),length);
}
void directory(const std::string&path) {
  auto f=file(path); GError*error=nullptr;
  auto info=Info::adopt(g_file_query_info(f.get(),G_FILE_ATTRIBUTE_STANDARD_TYPE,G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,nullptr,&error));
  if(info) {
    if(g_file_info_get_file_type(info.get())!=G_FILE_TYPE_DIRECTORY)
      throw std::runtime_error("Profile destination is not a real directory");
  } else if(!g_error_matches(error,G_IO_ERROR,G_IO_ERROR_NOT_FOUND)) {
    throw std::runtime_error(failure("Inspect profile directory",error));
  }
  g_clear_error(&error);
  auto parent=File::adopt(g_file_get_parent(f.get()));
  if(parent) {
    GimpPainter::String name(g_file_get_path(parent.get()));
    if(!name) throw std::runtime_error("Expected local profile directory");
    directory(name.get());
  }
  if(!info && !g_file_make_directory(f.get(),nullptr,&error)) {
    if(!g_error_matches(error,G_IO_ERROR,G_IO_ERROR_EXISTS))
      throw std::runtime_error(failure("Create profile directory",error));
    g_clear_error(&error);
    auto raced=Info::adopt(g_file_query_info(f.get(),G_FILE_ATTRIBUTE_STANDARD_TYPE,G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,nullptr,&error));
    if(!raced || g_file_info_get_file_type(raced.get())!=G_FILE_TYPE_DIRECTORY)
      throw std::runtime_error(failure("Profile directory changed during migration",error));
  }
}
/* Publish a complete file with G_FILE_COPY_NONE: no partially written active
 * config and no replacement of an existing edit or symlink on retries. */
bool write_new(const std::string&path,const std::string&bytes) {
  GimpPainter::String parent(g_path_get_dirname(path.c_str()));directory(parent.get());
  auto f=file(path);GError*error=nullptr;
  auto existing=Info::adopt(g_file_query_info(f.get(),G_FILE_ATTRIBUTE_STANDARD_TYPE,G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,nullptr,&error));
  if(existing) return false;
  if(!g_error_matches(error,G_IO_ERROR,G_IO_ERROR_NOT_FOUND)) throw std::runtime_error(failure("Inspect profile destination",error));
  g_clear_error(&error);
  GimpPainter::String id(g_uuid_string_random());
  auto temporary=file(join(parent.get(),std::string(".painter-migration-")+id.get()));
  auto out=GimpPainter::ObjectRef<GFileOutputStream>::adopt(g_file_create(temporary.get(),G_FILE_CREATE_PRIVATE,nullptr,&error));
  if(!out) throw std::runtime_error(failure("Create migration temporary file",error));
  bool ok=g_output_stream_write_all(G_OUTPUT_STREAM(out.get()),bytes.data(),bytes.size(),nullptr,nullptr,&error);
  if(ok) ok=g_output_stream_close(G_OUTPUT_STREAM(out.get()),nullptr,&error);
  out.reset();
  if(!ok) {g_file_delete(temporary.get(),nullptr,nullptr);throw std::runtime_error(failure("Write migration temporary file",error));}
  if(!g_file_move(temporary.get(),f.get(),G_FILE_COPY_NONE,nullptr,nullptr,nullptr,&error)) {
    const bool conflict=g_error_matches(error,G_IO_ERROR,G_IO_ERROR_EXISTS);
    g_file_delete(temporary.get(),nullptr,nullptr);
    if(conflict) {g_clear_error(&error);return false;}
    throw std::runtime_error(failure("Publish migrated profile",error));
  }
  return true;
}
std::string hash(const std::string&bytes) { GimpPainter::String digest(g_compute_checksum_for_data(G_CHECKSUM_SHA256,reinterpret_cast<const guchar*>(bytes.data()),bytes.size())); return digest.get(); }
struct Import {
  std::string source,destination,report;
  unsigned count=0;
  bool publish=true;
  void entry(const std::string&relative,const std::string&target,bool config,unsigned depth) {
    if(depth>64 || ++count>100000) throw std::runtime_error("Profile resource traversal limit exceeded");
    auto f=file(join(source,relative));GError*error=nullptr;
    auto info=Info::adopt(g_file_query_info(f.get(),G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,nullptr,&error));
    if(!info) { if(g_error_matches(error,G_IO_ERROR,G_IO_ERROR_NOT_FOUND)) {g_clear_error(&error);return;} throw std::runtime_error(failure("Inspect migration input",error)); }
    const auto type=g_file_info_get_file_type(info.get());
    if(type==G_FILE_TYPE_DIRECTORY) {
      if(publish)directory(join(destination,target));
      auto entries=Enumerator::adopt(g_file_enumerate_children(f.get(),G_FILE_ATTRIBUTE_STANDARD_NAME,G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,nullptr,&error));
      if(!entries) throw std::runtime_error(failure("Enumerate migration input",error));
      std::vector<std::string> names;
      while(auto item=Info::adopt(g_file_enumerator_next_file(entries.get(),nullptr,&error)))
        names.emplace_back(g_file_info_get_name(item.get()));
      if(error) throw std::runtime_error(failure("Enumerate migration input",error));
      std::sort(names.begin(),names.end());
      for(const auto&name:names) entry(join(relative,name),join(target,name),config,depth+1);
    } else if(type==G_FILE_TYPE_REGULAR) {
      auto bytes=read(join(source,relative));const auto digest=hash(bytes);
      const auto archive=join("painter-migration/originals",join(digest,relative));
      if(!write_new(join(destination,archive),bytes) && read(join(destination,archive))!=bytes) throw std::runtime_error("Original archive conflict; refusing migration");
      if(!publish) {report+="origin-unproven original "+quoted(archive)+"\n";return;}
      std::string converted=bytes;
      if(config) {
        try {converted=transform(bytes,relative,true);}
        catch(const std::exception&e) { report+="unparsed "+quoted(relative)+" "+quoted(e.what())+" original "+quoted(archive)+"\n";return; }
      }
      std::string mapped=target;
      if(relative=="tool-options/gimp-mypaint-tool") mapped="tool-options/gimp-painter-mypaint-tool";
      if(relative=="tool-options/gimp-smudge-tool") mapped="tool-options/gimp-painter-smudge-tool";
      if(relative=="tool-options/gimp-blend-tool") mapped="tool-options/gimp-gradient-tool";
      const bool saved=write_new(join(destination,mapped),converted);
      report+=(saved?"imported ":"existing-kept ")+quoted(mapped)+" original "+quoted(archive)+"\n";
    } else {
      const char*target=g_file_info_get_symlink_target(info.get());
      report+="not-followed "+quoted(relative)+" target "+quoted(target?target:"")+" (symlink or special file)\n";
    }
  }
};
} // namespace
extern "C" gchar*gimp_painter_profile_transform(const gchar*contents,const gchar*kind,gboolean origin,GError**error) {
  try { if(!contents||!kind) throw std::runtime_error("Missing profile transformation input");return g_strdup(transform(contents,kind,origin).c_str()); }
  catch(const std::exception&e) {g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_DATA,e.what());return nullptr;}
  catch(...) {g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,"Profile transformation failed");return nullptr;}
}
extern "C" gboolean gimp_painter_profile_detect(const gchar*path) {
  if(!path)return false;
  for(const auto*name:{"toolrc","contextrc","devicerc","tool-options/gimp-mypaint-tool","tool-options/gimp-smudge-tool"}) {
    try {
      const auto text=read(join(path,name));const auto nodes=Parser(text).parse();
      if(proof(nodes))return true;
      // The real old options writer omits the tool property. The namespaced
      // filename plus an actual Painter-only property is explicit evidence.
      for(const auto&n:nodes) {
        const auto k=key(n);const auto*a=argument(n);
        if(!a || n.children.size()!=2)continue;
        if(std::strcmp(name,"tool-options/gimp-mypaint-tool")==0 &&
           (k=="mypaint-brush" || k=="stroke-opacity" || k=="non-incremental"))return true;
        if(std::strcmp(name,"tool-options/gimp-smudge-tool")==0 && k=="use-color-blending" &&
           a->kind==Node::atom && (a->value=="yes" || a->value=="no"))return true;
      }
    } catch(...) {}
  }
  return false;
}
extern "C" gboolean gimp_painter_profile_handles(const gchar*name) {
  for(const auto*item:{"toolrc","contextrc","devicerc","gimprc","tool-options","tool-presets","mypaint-brushes","layer-presets"}) if(g_strcmp0(name,item)==0)return true;
  return false;
}
extern "C" gboolean gimp_painter_profile_preserve(const gchar*source,const gchar*destination,GError**error) {
  try {
    if(!source || !destination)throw std::runtime_error("Missing profile archive path");
    auto old_root=file(source),new_root=file(destination);
    if(g_file_equal(old_root.get(),new_root.get()) || g_file_has_prefix(new_root.get(),old_root.get()))
      throw std::runtime_error("Profile archive destination must be outside the source profile");
    Import importer{source,destination,"Painter profile migration v1: origin unproven. Original configuration retained; no Painter tool identities have been remapped.\n",0,false};
    for(const char*name:{"toolrc","contextrc","devicerc","tool-options","tool-presets","gimprc","menurc"})importer.entry(name,name,false,0);
    write_new(join(destination,join("painter-migration/reports",hash(importer.report)+".txt")),importer.report);
    return true;
  }catch(const std::exception&e){g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,e.what());return false;}
  catch(...){g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,"Profile archive failed");return false;}
}
extern "C" gboolean gimp_painter_profile_migrate(const gchar*source,const gchar*destination,GError**error) {
  try {
    if(!source||!destination||!gimp_painter_profile_detect(source)) throw std::runtime_error("Legacy Painter profile origin is not established");
    auto old_root=file(source), new_root=file(destination);
    if(g_file_equal(old_root.get(),new_root.get()) || g_file_has_prefix(new_root.get(),old_root.get()))
      throw std::runtime_error("Migration destination must be outside the source profile");
    directory(destination);
    Import importer{source,destination,"Painter profile migration v1\nOriginal bytes are retained permanently. Unknown fields may be ignored by modern loaders.\nExpanded tool-group state was not written by the pinned legacy serializer.\n",0};
    for(const auto*name:{"toolrc","contextrc","devicerc","gimprc","tool-options","tool-presets"}) importer.entry(name,name,true,0);
    // A proven Painter profile can legitimately omit gimprc entirely. The
    // no-overwrite writer preserves any destination created/edited previously.
    if(!g_file_query_exists(file(join(source,"gimprc")).get(),nullptr) &&
       write_new(join(destination,"gimprc"),"# Implicit legacy Painter canvas behavior\n(painter-canvas-ui yes)\n"))
      importer.report+="implicit-canvas-default from proven Painter origin\n";
    // save-tool-options was FALSE by default in the old application. Even a
    // marker-only migrated file must resolve native brush spacing at startup.
    auto old_toolrc=file(join(source,"toolrc"));
    if(g_file_query_exists(old_toolrc.get(),nullptr)) {
      try {
        const auto bytes=read(join(source,"toolrc"));std::vector<std::string> names;
        collect_brush_tools(Parser(bytes).parse(),names);
        for(const auto&name:names) {
          const auto path=join("tool-options",tool(name));
          std::string defaults="# Implicit defaults from legacy toolrc\n(paint-mode painter-normal)\n";
          if(old_brush_tool(name)) {
            if(name!="gimp-smudge-tool")defaults+="(painter-legacy-brush-geometry yes)\n";
            defaults+="(painter-legacy-native-spacing yes)\n";
            if(name!="gimp-smudge-tool")defaults+="(painter-legacy-native-hardness yes)\n";
          }
          if(write_new(join(destination,path),defaults))
            importer.report+="implicit-tool-defaults "+quoted(path)+" from archived toolrc\n";
        }
      }catch(const std::exception&e){importer.report+="toolrc-defaults-not-applied "+quoted(e.what())+"\n";}
    }
    importer.entry("mypaint-brushes","painter-mypaint-brushes",false,0);
    importer.entry("layer-presets","layer-presets",false,0);
    /* Shortcuts still use the upstream version-specific action map, with two
     * provenance-scoped Painter action aliases. Preserve its exact input. */
    for(const auto*name:{"menurc"}) {
      auto f=file(join(source,name));if(g_file_query_exists(f.get(),nullptr)) {auto bytes=read(join(source,name));write_new(join(destination,join("painter-migration/originals",join(hash(bytes),name))),bytes);}
    }
    write_new(join(destination,join("painter-migration/reports",hash(importer.report)+".txt")),importer.report);
    return true;
  } catch(const std::exception&e) {g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,e.what());return false;}
  catch(...) {g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,"Profile migration failed");return false;}
}
extern "C" const gchar*gimp_painter_profile_action(const gchar*action) {
  if(g_strcmp0(action,"tools-mypaint")==0)return "tools-painter-mypaint";
  if(g_strcmp0(action,"tools-smudge")==0)return "tools-painter-smudge";
  return action;
}

extern "C" gchar *gimp_painter_profile_resource_paths (const gchar *source, GError **error) {
  try {
    if (!source) throw std::runtime_error("Missing profile source");
    auto f=file(join(source,"gimprc"));
    if (!g_file_query_exists(f.get(),nullptr)) return g_strdup("");
    auto input=read(join(source,"gimprc")); const auto nodes=Parser(input).parse();
    std::string output;
    for (const auto& n:nodes) {
      const auto k=key(n); const auto*a=argument(n);
      if(!a || a->kind!=Node::string) continue;
      bool resource=false;
      for(const char*name:{"brush-path","dynamics-path","pattern-path","gradient-path","palette-path","tool-preset-path","mypaint-brush-path"})
        if(k==name) resource=true;
      if(!resource) continue;
      auto value=a->value;
      if(k=="mypaint-brush-path") {
        // Only the old application-variable components are relocated. Absolute
        // custom search paths stay usable; writable paths deliberately retain
        // their new-profile defaults to avoid editing the old originals.
        for(const char*prefix:{"${gimp_dir}/mypaint-brushes","${gimp_data_dir}/mypaint-brushes"}) {
          std::size_t at=0;
          while((at=value.find(prefix,at))!=std::string::npos) {
            const auto end=at+std::strlen(prefix);
            if((at==0 || value[at-1]==G_SEARCHPATH_SEPARATOR) &&
               (end==value.size() || value[end]==G_SEARCHPATH_SEPARATOR || value[end]=='/')) {
              const auto slash=value.find('/',at);
              value.replace(slash+1,std::strlen("mypaint-brushes"),"painter-mypaint-brushes");
            }
            ++at;
          }
        }
      }
      const char*family=k=="brush-path"?"brushes":k=="dynamics-path"?"dynamics":
        k=="pattern-path"?"patterns":k=="gradient-path"?"gradients":
        k=="palette-path"?"palettes":k=="tool-preset-path"?"tool-presets":"painter-mypaint-brushes";
      const std::string writable=std::string("${gimp_dir}/")+family;
      bool has_writable=false;std::size_t begin=0;
      while(begin<=value.size()) {
        auto end=value.find(G_SEARCHPATH_SEPARATOR,begin);if(end==std::string::npos)end=value.size();
        has_writable|=value.substr(begin,end-begin)==writable;
        if(end==value.size())break;
        begin=end+1;
      }
      if(!has_writable) {if(!value.empty())value+=G_SEARCHPATH_SEPARATOR;value+=writable;}
      output+="("+(k=="mypaint-brush-path"?std::string("painter-mypaint-brush-path"):k)+" "+quoted(value)+")\n";
    }
    return g_strdup(output.c_str());
  } catch(const std::exception&e) {g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_DATA,e.what());return nullptr;}
  catch(...) {g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,"Resource path migration failed");return nullptr;}
}
