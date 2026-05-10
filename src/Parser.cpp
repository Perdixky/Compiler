module;
#include <proxy/proxy.h>
export module Parser;
import std;
import SourceLocation;

struct Node {
  SourceLocation location;
};
