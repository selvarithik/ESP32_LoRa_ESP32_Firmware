#include "../src/communication/http_body_core.h"
#include <string>
#include <cassert>
#include <cstdio>
struct Client{
 std::string body;size_t offset=0;bool alive=true;uint32_t* now=nullptr;bool trickle=false;
 int available(){return trickle&&*now%3?0:int(body.size()-offset);}
 bool connected(){return alive;}
 int read(uint8_t* out,size_t n){if(trickle&&n>1)n=1;for(size_t i=0;i<n;++i)out[i]=body[offset++];if(trickle)++*now;return int(n);}
};
struct Sink{std::string body;bool reject=false;size_t write(const uint8_t* data,size_t n){if(reject)return 0;body.append((const char*)data,n);return n;}};
int main(){
 uint32_t now=0;auto clock=[&](){return now;};auto pause=[&](){++now;};
 Client never;Sink a;assert(!HttpBodyCore::read(never,a,100,512,20,clock,pause));assert(now==20);
 now=0;Client truncated;truncated.body="abc";truncated.alive=false;Sink b;assert(!HttpBodyCore::read(truncated,b,10,512,20,clock,pause));assert(b.body=="abc");
 Client exact;exact.body="{\"ok\":true}";Sink c;assert(HttpBodyCore::read(exact,c,int(exact.body.size()),512,20,clock,pause));assert(c.body==exact.body);
 Client oversize;Sink d;assert(!HttpBodyCore::read(oversize,d,513,512,20,clock,pause));assert(!HttpBodyCore::read(oversize,d,-1,512,20,clock,pause));assert(!HttpBodyCore::read(oversize,d,0,512,20,clock,pause));
 now=0;Client slow;slow.body=std::string(100,'x');slow.now=&now;slow.trickle=true;Sink e;assert(!HttpBodyCore::read(slow,e,100,512,20,clock,pause));assert(now==20);
 now=0xfffffff8U;Client wrap;Sink f;assert(!HttpBodyCore::read(wrap,f,1,512,20,clock,pause));assert(now==12);
 Client shortSink;shortSink.body="x";Sink g;g.reject=true;assert(!HttpBodyCore::read(shortSink,g,1,512,20,clock,pause));
 puts("PASS production HTTP body reader: persistent empty socket, truncation, exact length, size bounds, unknown length, slow trickle, timer wrap, sink failure");
}
