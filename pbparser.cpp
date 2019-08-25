#include <iostream>
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include <typeinfo>
#include <string>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/compiler/importer.h"
#include <vector>
#include <cstdio>
#include <fstream>
#include <ctime>

using namespace std;
using namespace rapidjson;
using namespace google::protobuf::compiler;
using namespace google::protobuf;
				
#define JSON_CASE_OPERATION(method,valuetype) \
                    rapidjson::Value str_key(field->name().c_str(),allocator); \
                    valuetype value = reflection->Get##method(message,field); \
                    d.AddMember(str_key,value,allocator);

ofstream fout;

class MockErrorCollector : public MultiFileErrorCollector {
 public:
  MockErrorCollector() {}
  ~MockErrorCollector() {}

  // implements ErrorCollector ---------------------------------------
  void AddError(const string& filename, int line, int column,
                const string& message) {
    char sMsg[40960];
    snprintf(sMsg,sizeof(sMsg) , "finleName: %s ,line: %d , column: %d , error: %s\n" , filename.c_str(), line , column , message.c_str());
    text_ += sMsg;
  }
  void  GetErrorAndClear(string& errorStr)
  {
    errorStr = text_;
    text_ = "";
  }
private:
    string text_;
};



vector<string> shortStr(string fullname){
    vector<string> vStr;
    size_t pos = fullname.find(".");
    while(pos != string::npos ){
        vStr.push_back(fullname.substr(0,pos));
        fullname = fullname.substr(pos+1);
        pos = fullname.find(".");
    }
    vStr.push_back(fullname);
    return vStr;
}

string checkMessage(Importer* importer, string fullname){
    const Descriptor *descriptor = importer->pool()->FindMessageTypeByName(fullname);
    vector<string> names = shortStr(fullname);
    if(NULL == descriptor){
        if(names.size() == 1){
            return "";
        }
        else if(names.size() == 2){
            descriptor = importer->pool()->FindMessageTypeByName(names[1]);
            if(NULL != descriptor){
                return names[1];    
            }
            else{
                return "";    
            }
        }
        else{
            string tmp="";
            for(size_t i=0;i<names.size()-2;++i){
                tmp += names[i];
                tmp += ".";
            }
            tmp += names[names.size()-1];
            return checkMessage(importer,tmp);
        }
    }
    else{
        return fullname;
    }
}

void dealSub(Importer* importer, string fullname,string& jsonStr,string defaultVal="string"){
    const Descriptor *descriptor = importer->pool()->FindMessageTypeByName(fullname);
    fout<<"Deal SubMessage:"<<endl;
    if(NULL != descriptor){
        fout<<" == message full name:"+fullname<<endl;
    }
    else{
        fout<<"Cannot found message:";
        fout<<fullname<<endl;
        return;    
    }
    int size = descriptor->field_count();
    Document dSub;
    dSub.SetObject();
    if(size <= 0 ){
        return;
    }
    
    for (int i = 0; i < size;++i){
        const google::protobuf::FieldDescriptor* field = descriptor->field(i);
        if(NULL != field){
            Value str_key(field->name().c_str(),dSub.GetAllocator());
            if(string("message") == field->type_name()){
                Document str_val;//(kStringType);
                string subMessageName =  field->message_type()->full_name();//fullname + "." + field->message_type()->name();
                //const Descriptor *subDescriptor = importer->pool()->FindMessageTypeByName(subMessageName);
                //subMessageName = checkMessage(importer,subMessageName);
                if("" == subMessageName){
                    return;    
                }
                string subJsonStr="";
                dealSub(importer,subMessageName,subJsonStr,defaultVal);
                str_val.Parse(subJsonStr.c_str());
                if(!str_val.HasParseError()){
                    dSub.AddMember(str_key,str_val,dSub.GetAllocator());    
                }
                else{
                    Value tmp(defaultVal.c_str(),dSub.GetAllocator());
                    dSub.AddMember(str_key,tmp,dSub.GetAllocator());
                }
            }
            else{
                Value str_val(kStringType);
                str_val.SetString(defaultVal.c_str(),defaultVal.size());
                dSub.AddMember(str_key,str_val,dSub.GetAllocator());
            }
        }
    }

    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    dSub.Accept(writer);
    jsonStr = sb.GetString();
}

int ImportPbFile(const string& fileName,const string& filepath,const string& msgName , string& outJsonStr, string defaultVal="string"){
    DiskSourceTree sourceTree;
    sourceTree.MapPath("",filepath);
    MockErrorCollector errCollector;
    Importer* importer = new Importer(&sourceTree, &errCollector);
    //importer->Import("test3.proto");
    if (NULL == importer->Import(fileName))
    {
        fout<<"parse proto file: "+fileName +" failed!"<<endl;
        fout<<"err:";
        string err;
        errCollector.GetErrorAndClear(err);
        fout<<err<<endl;
        delete importer;
        importer = NULL;
        return -1;
    }
    const google::protobuf::FileDescriptor* pFileDesc = importer->pool()->FindFileByName(fileName);
    
    const string& packageName = pFileDesc->package();
    fout<<"==========================="<<endl;
    fout<<pFileDesc->dependency_count()<<endl;
    for(int i=0;i<pFileDesc->dependency_count();++i){
        const google::protobuf::FileDescriptor* imported = pFileDesc->dependency(i);
        fout<<"import:";
        fout<<imported->name()<<endl;
    }
    fout<<"==========================="<<endl;

    string fullname;
    if(""!=packageName){
        fullname = packageName+"."+msgName;
    }
    else
        fullname = msgName;
   
    Document d;
    if("" == outJsonStr){
        d.SetObject();
    }
    else{
        d.Parse(outJsonStr.c_str());
    }   
    Document::AllocatorType& allocator = d.GetAllocator();
    const Descriptor *descriptor = importer->pool()->FindMessageTypeByName(fullname);
    if(NULL == descriptor){
        fout<<"Parse message: "+fullname+" failed!"<<endl;    
        return -2;
    }
    int size = descriptor->field_count();
    if(size <= 0){
        fout<<"Parse member params of message: "+fullname+" failed!"<<endl;
        return -3;
    }
    for (int i = 0; i < size;++i){
        const google::protobuf::FieldDescriptor* field = descriptor->field(i);
        if(NULL != field){
            fout<<"Parse Param:";
            fout<<field->name()<<endl;
            if(field->is_required() || field->is_optional() ){
                Value str_key(field->name().c_str(),allocator);
                if(string("message") == field->type_name()){
                    Document str_val;//(kObjectType);
                    string subMessageName = field->message_type()->full_name();//fullname + "." + field->message_type()->name();
                    //const Descriptor *subDescriptor = importer->pool()->FindMessageTypeByName(subMessageName);
                    //subMessageName = checkMessage(importer,subMessageName);
                    if("" == subMessageName){
                        continue;
                    }
                    string subJsonStr="";
                    dealSub(importer,subMessageName,subJsonStr,defaultVal);
                    str_val.Parse(subJsonStr.c_str());
                    if(!str_val.HasParseError()){
                        d.AddMember(str_key,str_val,allocator);
                    }
                    else{
                        Value tmp(defaultVal.c_str(),allocator);
                        d.AddMember(str_key,tmp,allocator); 
                    }
                }
                else{
                    Value str_val(kStringType);
                    str_val.SetString(defaultVal.c_str(),defaultVal.size());
                    d.AddMember(str_key,str_val,allocator);
                }
            }
            if(field->is_repeated()){
                rapidjson::Value str_key(field->name().c_str(),allocator);
                if(string("message") == field->type_name()){
                    Value str_val;
                    str_val.SetArray();
                    string subMessageName = field->message_type()->full_name();// fullname + "." + field->message_type()->name();
                    if("" == subMessageName){
                        continue;
                    }
                    string subJsonStr="";
                    dealSub(importer,subMessageName,subJsonStr,"string");
                    Document strArry_val;
                    strArry_val.Parse(subJsonStr.c_str());
                    Value swapVal;
                    swapVal.SetObject();
                    swapVal.CopyFrom(strArry_val,d.GetAllocator());
                    if(!strArry_val.HasParseError()){
                        str_val.PushBack(swapVal,d.GetAllocator());
                    }
                    else{
                        Value tmp(defaultVal.c_str(),allocator);
                        str_val.PushBack(tmp,allocator);    
                    }
                    d.AddMember(str_key,str_val,allocator);
                }
                else{
                    Value str_val(kArrayType);
                    Value strArry_val(kStringType);
                    strArry_val.SetString(defaultVal.c_str(),defaultVal.size());
                    str_val.PushBack(strArry_val,allocator);
                    d.AddMember(str_key,str_val,allocator);
                }
            }
        }
    }

    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    d.Accept(writer);
    //fout<<sb.GetString()<<endl;
    outJsonStr = sb.GetString();
    return 0;
}

string nowday(){
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buffer[20];
    int year = 1900 + ltm->tm_year;
    int month = 1 + ltm->tm_mon;
    int day = ltm->tm_mday;

    if(month < 10 && day < 10 ){
        snprintf(buffer,9,"%d0%d0%d",year,month,day);
    }
    else if(month>=10 && day<10){
        snprintf(buffer,9,"%d%d0%d",year,month,day);
    }
    else if(month<10 && day>=10){
        snprintf(buffer,9,"%d0%d%d",year,month,day);
    }
    else{
         snprintf(buffer,9,"%d%d%d",year,month,day);
    }
    return string(buffer);
}

int main(int argc,char ** argv){
    string proto_file_name;
    string proto_file_path;
    string message_name;
    
    fout.open(("./log."+nowday()).c_str(),std::ofstream::out | std::ofstream::app);
    fout<<"Parse Begin:"<<endl;
    time_t now = time(0);
    fout<<ctime(&now)<<endl;
    if( 4 > argc ){
        cout<<"usage:"<<endl;
        cout<<"    pbparser proto_file_name proto_file_path message_name "<<endl;
        cout<<"        proto_file_name: proto file name, eg:test.proto"<<endl;
        cout<<"        proto_file_path: proto file path, eg: ./"<<endl;
        cout<<"        message_name: proto message name"<<endl;

        cout<<"eg:"<<endl;
        cout<<"    file: ./test3.proto"<<endl;
        cout<<"        \"syntax = \"proto2\";"<<endl;
        cout<<"          package tutorial;"<<endl;
        
        cout<<"          message Person {"<<endl;
        cout<<"              required string name = 1;"<<endl;
        cout<<"              required int32 id = 2;"<<endl;
        cout<<"              optional string email = 3;"<<endl;
        cout<<"          }\""<<endl;

        cout<<"    cmd: pbparser  test3.proto  ./  Person"<<endl;
        cout<<endl;
        return 1;
    }
    else{
        proto_file_name = argv[1];
        proto_file_path = argv[2];
        message_name = argv[3];
    //    cout<<proto_file_name<<endl;
    //    cout<<proto_file_path<<endl;
    //    cout<<message_name<<endl;
    }
    
    string out;
    //ImportPbFile("test.proto","./","AddressBook",out);
    ImportPbFile(proto_file_name,proto_file_path,message_name,out);
    cout<<out<<endl;
    fout<<out<<endl;
    fout<<"Parse Finish!"<<endl;
    fout.close();
    return 0;
}

