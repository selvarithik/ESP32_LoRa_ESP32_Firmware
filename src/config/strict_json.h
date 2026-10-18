#pragma once
#include <ArduinoJson.h>
#include <vector>
// Preflight before ArduinoJson (which otherwise accepts duplicate keys).
namespace StrictJson {
struct Reader {
 const char* p; const char* end;
 void ws(){while(p<end&&(*p==' '||*p=='\r'||*p=='\n'||*p=='\t'))++p;}
 bool string(String* key=nullptr){const char* start=p;if(p==end||*p++!='"')return false;
  while(p<end){unsigned char c=*p++;if(c=='"'){if(key){JsonDocument d;if(deserializeJson(d,start,p-start))return false;*key=d.as<String>();}return true;}
   if(c<32)return false;if(c=='\\'){if(p==end)return false;c=*p++;if(c=='u'){unsigned code=0;for(int i=0;i<4;i++){if(p==end||!isxdigit(*p))return false;char h=*p++;code=code*16+(h<='9'?h-'0':tolower(h)-'a'+10);}if(code==0)return false;}else if(!strchr("\"\\/bfnrt",c))return false;}
   else if(c>=128){int n=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:-1;if(n<0||end-p<n)return false;unsigned char first=*p;if((c==0xe0&&first<0xa0)||(c==0xed&&first>=0xa0)||(c==0xf0&&first<0x90)||(c==0xf4&&first>=0x90))return false;while(n--)if(((*p++)&0xc0)!=0x80)return false;}
  }return false;
 }
 bool value(int depth){ws();if(depth>8||p==end)return false;if(*p=='"')return string();
  if(*p=='{'||*p=='['){bool obj=*p++=='{';char close=obj?'}':']';std::vector<String> keys;ws();if(p<end&&*p==close){++p;return true;}for(;;){if(obj){String key;if(!string(&key))return false;for(auto& k:keys)if(k==key)return false;keys.push_back(key);ws();if(p==end||*p++!=':')return false;}if(!value(depth+1))return false;ws();if(p==end)return false;if(*p==close){++p;return true;}if(*p++!=',')return false;ws();}}
  for(auto literal:{"true","false","null"}){size_t n=strlen(literal);if(size_t(end-p)>=n&&!strncmp(p,literal,n)){p+=n;return true;}}
  if(*p=='-')++p;if(p==end||!isdigit(*p))return false;if(*p=='0')++p;else while(p<end&&isdigit(*p))++p;
  if(p<end&&*p=='.'){++p;if(p==end||!isdigit(*p))return false;while(p<end&&isdigit(*p))++p;}
  if(p<end&&(*p=='e'||*p=='E')){++p;if(p<end&&(*p=='+'||*p=='-'))++p;if(p==end||!isdigit(*p))return false;while(p<end&&isdigit(*p))++p;}return true;
 }
};
inline bool valid(const String& raw,size_t max=6144){if(raw.isEmpty()||raw.length()>max||strlen(raw.c_str())!=raw.length())return false;Reader r{raw.c_str(),raw.c_str()+raw.length()};if(!r.value(0))return false;r.ws();return r.p==r.end;}
}
