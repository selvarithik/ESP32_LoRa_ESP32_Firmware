#include "../src/web/session_core.h"
#include <assert.h>
#include <stdio.h>
#include <string>
int main(){
 BrowserSessions sessions;std::string token(64,'a'),csrf(32,'b');
 auto* anonymous=sessions.create(token.c_str(),csrf.c_str(),100,1,false);
 assert(anonymous&&!anonymous->authenticated);
 assert(BrowserSessions::same(anonymous->csrf,csrf.c_str()));
 assert(!BrowserSessions::same(anonymous->csrf,"wrong"));
 sessions.revoke(anonymous);assert(!sessions.find(token.c_str(),101,1));
 auto* login=sessions.create(token.c_str(),csrf.c_str(),200,1,true);assert(login->authenticated);
 assert(sessions.find(token.c_str(),899999,1));
 assert(!sessions.find(token.c_str(),1800000,1));
 sessions.create(token.c_str(),csrf.c_str(),1,2,true);assert(!sessions.find(token.c_str(),2,3));
 sessions.create(token.c_str(),csrf.c_str(),0xfffffff0,3,true);assert(sessions.find(token.c_str(),40,3));
 assert(!sessions.find(token.c_str(),900041,3));
 for(unsigned i=0;i<5;i++){std::string t(64,char('a'+i));assert(sessions.create(t.c_str(),csrf.c_str(),i,4,true));}
 assert(!sessions.find(token.c_str(),10,4));
 assert(!sessions.create("short",csrf.c_str(),10,4,true));
 puts("PASS production session core: CSRF comparison, revocation, idle expiry/sliding, credential generation, timer wrap, bounded eviction");
}
