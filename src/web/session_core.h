#pragma once
#include <stdint.h>
#include <string.h>
// Portable production state machine, tested with the native host compiler.
class BrowserSessions {
public:
    struct Entry {char token[65]{};char csrf[33]{};uint32_t touched=0,generation=0;bool authenticated=false;};
    Entry* find(const char* token,uint32_t now,uint32_t generation){
        for(auto& e:entries){
            if(e.token[0]&&(uint32_t(now-e.touched)>=900000||e.generation!=generation))e=Entry{};
            if(e.token[0]&&same(e.token,token)){e.touched=now;return &e;}
        }return nullptr;
    }
    Entry* create(const char* token,const char* csrf,uint32_t now,uint32_t generation,bool authenticated){
        if(strlen(token)!=64||strlen(csrf)!=32)return nullptr;
        Entry* selected=&entries[0];
        for(auto& e:entries){if(!e.token[0]||uint32_t(now-e.touched)>=900000||e.generation!=generation){selected=&e;break;}if(uint32_t(now-e.touched)>uint32_t(now-selected->touched))selected=&e;}
        *selected=Entry{};strcpy(selected->token,token);strcpy(selected->csrf,csrf);selected->touched=now;selected->generation=generation;selected->authenticated=authenticated;return selected;
    }
    void revoke(Entry* e){if(e)*e=Entry{};}
    static bool same(const char* a,const char* b){if(!a||!b)return false;unsigned diff=strlen(a)^strlen(b);for(unsigned i=0;a[i]&&b[i];++i)diff|=a[i]^b[i];return diff==0;}
private:Entry entries[4];
};
