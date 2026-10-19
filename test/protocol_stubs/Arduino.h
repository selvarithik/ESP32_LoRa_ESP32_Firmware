#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>
#include <cstdio>
#include <cctype>
class String {
 std::string value;
public:
 String()=default;
 String(const char* s):value(s?s:""){}
 String(const std::string& s):value(s){}
 String(const String&)=default;
 String& operator=(const String&)=default;
 String& operator=(const char* s){value=s?s:"";return *this;}
 const char* c_str()const{return value.c_str();}
 size_t length()const{return value.length();}
 bool isEmpty()const{return value.empty();}
 bool reserve(size_t n){value.reserve(n);return true;}
 char operator[](size_t n)const{return value[n];}
 bool concat(const char* s,size_t n){value.append(s,n);return true;}
 bool concat(const char* s){value.append(s);return true;}
 bool concat(char c){value.push_back(c);return true;}
 bool operator==(const String& s)const{return value==s.value;}
 bool operator!=(const String& s)const{return value!=s.value;}
 std::string::const_iterator begin()const{return value.begin();}
 std::string::const_iterator end()const{return value.end();}
};
