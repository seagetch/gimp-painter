#include <typeinfo>
#include <map>
#include <cassert>
#include <cstdio>
using gconstpointer=const void*;
struct PresetConfig { virtual ~PresetConfig()=default; };
struct First:PresetConfig{}; struct Second:PresetConfig{};
struct Registry { std::map<const void*,PresetConfig*> values; void insert(const void*k,PresetConfig*v){values[k]=v;} };
struct PresetFactory { Registry data; Registry& registry(){return data;} void register_config(PresetConfig* config); };
void
PresetFactory::register_config (PresetConfig* config)
{
  gconstpointer type_ptr = &typeid(config);
  registry().insert(type_ptr, config);
}
int main(){PresetFactory f;First a;Second b;assert(f.registry().values.empty());f.register_config(&a);assert(f.registry().values.size()==1);assert(f.registry().values.begin()->second==&a);f.register_config(&b);assert(f.registry().values.size()==1);assert(f.registry().values.begin()->second==&b);f.register_config(&b);assert(f.registry().values.size()==1);assert(typeid(static_cast<PresetConfig*>(&a))==typeid(static_cast<PresetConfig*>(&b)));assert(typeid(a)!=typeid(b));puts("PASS exact original register_config body: empty, first, static-pointer-key replacement, repeat, distinct dynamic types");}
