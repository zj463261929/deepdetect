/**
 * DeepDetect
 * Copyright (c) 2014-2015 Emmanuel Benazera
 * Author: Emmanuel Benazera <beniz@droidnik.fr>
 *
 * This file is part of deepdetect.
 *
 * deepdetect is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * deepdetect is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with deepdetect.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "httpjsonapi.h"
#include "utils/utils.hpp"
#include <algorithm>
#include <csignal>
#include <iostream>
#include "ext/rmustache/mustache.h"
#include "ext/rapidjson/document.h"
#include "ext/rapidjson/stringbuffer.h"
#include "ext/rapidjson/reader.h"
#include "ext/rapidjson/writer.h"
#include <gflags/gflags.h>
#include "utils/httpclient.hpp"
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/copy.hpp>
#include <chrono>
#include <ctime>

DEFINE_string(host,"localhost","host for running the server");
DEFINE_string(port,"8080","server port");
DEFINE_int32(nthreads,20,"number of HTTP server threads");

using namespace boost::iostreams;

namespace dd
{
  std::string uri_query_to_json(const std::string &req_query)
  {
	if (!req_query.empty())
	  {
	JDoc jd;
	JVal jsv(rapidjson::kObjectType);
	jd.SetObject();
	std::vector<std::string> voptions = dd::dd_utils::split(req_query,'&');
	for (const std::string o: voptions)
	  {
		std::vector<std::string> vopt = dd::dd_utils::split(o,'=');
		if (vopt.size() == 2)
		  {
		bool is_word = false;
		for (size_t i=0;i<vopt.at(1).size();i++)
		  {
			if (isalpha(vopt.at(1)[i]))
			  {
			is_word = true;
			break;
			  }
		  }
		// we break '.' into JSON sub-objects
		std::vector<std::string> vpt = dd::dd_utils::split(vopt.at(0),'.');
		JVal jobj(rapidjson::kObjectType);
		if (vpt.size() > 1)
		  {
			bool bt = dd::dd_utils::iequals(vopt.at(1),"true");
			bool bf = dd::dd_utils::iequals(vopt.at(1),"false");
			if (is_word && !bt && !bf)
			  {
			jobj.AddMember(JVal().SetString(vpt.back().c_str(),jd.GetAllocator()),JVal().SetString(vopt.at(1).c_str(),jd.GetAllocator()),jd.GetAllocator());
			  }
			else if (bt || bf)
			  {
			jobj.AddMember(JVal().SetString(vpt.back().c_str(),jd.GetAllocator()),JVal(bt ? true : false),jd.GetAllocator());
			  }
			else jobj.AddMember(JVal().SetString(vpt.back().c_str(),jd.GetAllocator()),JVal(atoi(vopt.at(1).c_str())),jd.GetAllocator());
			for (int b=vpt.size()-2;b>0;b--)
			  {
			JVal jnobj(rapidjson::kObjectType);
			jobj = jnobj.AddMember(JVal().SetString(vpt.at(b).c_str(),jd.GetAllocator()),jobj,jd.GetAllocator());
			  }
			jsv.AddMember(JVal().SetString(vpt.at(0).c_str(),jd.GetAllocator()),jobj,jd.GetAllocator());
		  }
		else
		  {
			bool bt = dd::dd_utils::iequals(vopt.at(1),"true");
			bool bf = dd::dd_utils::iequals(vopt.at(1),"false");
			if (is_word && !bt && !bf)
			  {
			jsv.AddMember(JVal().SetString(vopt.at(0).c_str(),jd.GetAllocator()),JVal().SetString(vopt.at(1).c_str(),jd.GetAllocator()),jd.GetAllocator());
			  }
			else if (bt || bf)
			  {
			jsv.AddMember(JVal().SetString(vopt.at(0).c_str(),jd.GetAllocator()),JVal(bt ? true : false),jd.GetAllocator());
			  }
			else jsv.AddMember(JVal().SetString(vopt.at(0).c_str(),jd.GetAllocator()),JVal(atoi(vopt.at(1).c_str())),jd.GetAllocator());
		  }
		  }
	  }
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	jsv.Accept(writer);
	return buffer.GetString();
	  }
	else return "";
  }
}

class APIHandler
{
public:
  APIHandler(dd::HttpJsonAPI *hja)
	:_hja(hja) { }
  
  ~APIHandler() { }

  /*static boost::network::http::basic_response<std::string> cstock_reply(int status,
									std::string const& content) {
	using boost::lexical_cast;
	boost::network::http::basic_response<std::string> rep;
	rep.status = status;
	rep.content = content;
	rep.headers.resize(2);
	rep.headers[0].name = "Content-Length";
	rep.headers[0].value = lexical_cast<std::string>(rep.content.size());
	rep.headers[1].name = "Content-Type";
	rep.headers[1].value = "text/html";
	return rep;
	}*/
  
  void fillup_response(http_server::response &response,
			   const JDoc &janswer,
			   std::string &access_log,
			   int &code,
			   std::chrono::time_point<std::chrono::system_clock> tstart,
			   const std::string &encoding="")
  {
	std::chrono::time_point<std::chrono::system_clock> tstop = std::chrono::system_clock::now();
	std::string service;
	if (janswer.HasMember("head"))
	  {
	if (janswer["head"].HasMember("service"))
	  service = janswer["head"]["service"].GetString();
	  }
	if (!service.empty())
	  access_log += " " + service;
	code = janswer["status"]["code"].GetInt();
	access_log += " " + std::to_string(code);
	int proctime = std::chrono::duration_cast<std::chrono::milliseconds>(tstop-tstart).count();
	access_log += " " + std::to_string(proctime);
	int outcode = code;
	std::string stranswer;
	if (janswer.HasMember("template")) // if output template, fillup with rendered template.
	  {
	std::string tpl = janswer["template"].GetString();
	std::stringstream sg;
	mustache::RenderTemplate(tpl," ",janswer,&sg);
	stranswer = sg.str();
	  }
	else
	  {
	stranswer = _hja->jrender(janswer);
	  }
	if (janswer.HasMember("network"))
	  {
	//- grab network call parameters
	std::string url,http_method="POST",content_type="Content-Type: application/json";
	if (janswer["network"].HasMember("url"))
	  url = janswer["network"]["url"].GetString();
	if (url.empty())
	  {
		LOG(ERROR) << "missing url in network output connector";
		stranswer = _hja->jrender(_hja->dd_bad_request_400());
		code = 400;
	  }
	else
	  {
		if (janswer["network"].HasMember("http_method"))
		  http_method = janswer["network"]["http_method"].GetString();
		if (janswer["network"].HasMember("content_type"))
		  content_type = janswer["network"]["content_type"].GetString();
		
		//- make call
		std::string outstr;
		try
		  {
		dd::httpclient::post_call(url,stranswer,http_method,outcode,outstr,content_type);
		stranswer = outstr;
		  }
		catch (std::runtime_error &e)
		  {
		LOG(ERROR) << e.what() << std::endl;
		LOG(INFO) << stranswer << std::endl;
		stranswer = _hja->jrender(_hja->dd_output_connector_network_error_1009());
		  }
	  }
	  }
	bool has_gzip = (encoding.find("gzip") != std::string::npos);
	if (!encoding.empty() && has_gzip)
	  {
	try
	  {
		std::string gzstr;
		filtering_ostream gzout;
		gzout.push(gzip_compressor());
		gzout.push(boost::iostreams::back_inserter(gzstr));
		gzout << stranswer;
		boost::iostreams::close(gzout);
		stranswer = gzstr;
	  }
	catch(const std::exception &e)
	  {
		LOG(ERROR) << e.what() << std::endl;
		outcode = 400;
		stranswer = _hja->jrender(_hja->dd_bad_request_400());
	  }
	  }
	response = http_server::response::stock_reply(http_server::response::status_type(outcode),stranswer);
	response.headers[1].value = "application/json";
	if (!encoding.empty() && has_gzip)
	  {
	response.headers.resize(3);
	response.headers[2].name = "Content-Encoding";
	response.headers[2].value = "gzip";
	  }
	response.status = static_cast<http_server::response::status_type>(code);
  }
//2017-11-18 modify by lg start
void fillup_response_branch(const std::string &body,
			   http_server::response &response,
			   const JDoc &janswer,
			   std::string &access_log,
			   int &code,
			   std::chrono::time_point<std::chrono::system_clock> tstart,
			   const std::string &encoding="")
  {
	rapidjson::Document d;
	d.Parse(body.c_str());
	//if (d.HasParseError())
	//{
	//	  LOG(ERROR) << "JSON parsing error on string: " << body << std::endl;
	//	  return dd_bad_request_400();
	//}
	//APIData ad;	
	//ad = APIData(d);
	bool bIsMtcnn	  = false;
	bool bIsEmotion	  = false;
	bool bIsAttention = false;
	if(d.IsObject())
	{
		if ( d.HasMember("mtcnn") && d.HasMember("emotion") && d.HasMember("attention") )
		{
			if( d["mtcnn"].HasMember("enable") && d["emotion"].HasMember("enable") && d["attention"].HasMember("enable") )
			{
				if( d["mtcnn"]["enable"].IsBool() && d["emotion"]["enable"].IsBool() && d["attention"]["enable"].IsBool() )
				{
					bIsMtcnn	 = d["mtcnn"]["enable"].GetBool();
					bIsEmotion	 = d["emotion"]["enable"].GetBool();
					bIsAttention = d["attention"]["enable"].GetBool();
				}
			}
		}
	}
	if( bIsMtcnn && bIsEmotion && bIsAttention )
	{
		fillup_response_mtcnn_emotion_attention(response,janswer,access_log,code,tstart,encoding);
	}
	else
	{
		fillup_response(response,janswer,access_log,code,tstart,encoding);
	}
  }
void fillup_response_mtcnn_emotion_attention(http_server::response &response,
			   const JDoc &janswer,
			   std::string &access_log,
			   int &code,
			   std::chrono::time_point<std::chrono::system_clock> tstart,
			   const std::string &encoding="")
  {
	std::chrono::time_point<std::chrono::system_clock> tstop = std::chrono::system_clock::now();
	std::string service;
	double time;
	if (janswer.HasMember("head"))
	  {
		if (janswer["head"].HasMember("service"))
		{
			service = janswer["head"]["service"].GetString();
		}
		//2017-11-20 add time
		if (janswer["head"].HasMember("time"))
		{
			if(janswer["head"]["time"].IsDouble())
			{
				time = janswer["head"]["time"].GetDouble();
			}
		}
	  }
	if (!service.empty())
	  access_log += " " + service;
	code = janswer["status"]["code"].GetInt();
	access_log += " " + std::to_string(code);
	int proctime = std::chrono::duration_cast<std::chrono::milliseconds>(tstop-tstart).count();
	access_log += " " + std::to_string(proctime);
	int outcode = code;
	std::string stranswer;
	if (janswer.HasMember("template")) // if output template, fillup with rendered template.
	  {
	std::string tpl = janswer["template"].GetString();
	std::stringstream sg;
	mustache::RenderTemplate(tpl," ",janswer,&sg);
	stranswer = sg.str();
	  }
	else
	  {
	stranswer = _hja->jrender(janswer);
	  }
	if (janswer.HasMember("network"))
	  {
	//- grab network call parameters
	std::string url,http_method="POST",content_type="Content-Type: application/json";
	if (janswer["network"].HasMember("url"))
	  url = janswer["network"]["url"].GetString();
	if (url.empty())
	  {
		LOG(ERROR) << "missing url in network output connector";
		stranswer = _hja->jrender(_hja->dd_bad_request_400());
		code = 400;
	  }
	else
	  {
		if (janswer["network"].HasMember("http_method"))
		  http_method = janswer["network"]["http_method"].GetString();
		if (janswer["network"].HasMember("content_type"))
		  content_type = janswer["network"]["content_type"].GetString();
		
		//- make call
		std::string outstr;
		try
		  {
		dd::httpclient::post_call(url,stranswer,http_method,outcode,outstr,content_type);
		stranswer = outstr;
		  }
		catch (std::runtime_error &e)
		  {
		LOG(ERROR) << e.what() << std::endl;
		LOG(INFO) << stranswer << std::endl;
		stranswer = _hja->jrender(_hja->dd_output_connector_network_error_1009());
		  }
		}
	}
	  
	//2017-11-18 modify by lg
	try
	{
		result_analysis(time,stranswer);
	}
	catch(const std::exception &e)
	{
		LOG(ERROR) << e.what() << std::endl;
		outcode = 400;
		stranswer = _hja->jrender(_hja->dd_bad_request_400());
	}
	
	bool has_gzip = (encoding.find("gzip") != std::string::npos);
	if (!encoding.empty() && has_gzip)
	{
		try
		{
			std::string gzstr;
			filtering_ostream gzout;
			gzout.push(gzip_compressor());
			gzout.push(boost::iostreams::back_inserter(gzstr));
			gzout << stranswer;
			boost::iostreams::close(gzout);
			stranswer = gzstr;
		}
		catch(const std::exception &e)
		{
			LOG(ERROR) << e.what() << std::endl;
			outcode = 400;
			stranswer = _hja->jrender(_hja->dd_bad_request_400());
		}
	}
	
	response = http_server::response::stock_reply(http_server::response::status_type(outcode),stranswer);
	response.headers[1].value = "application/json";
	if (!encoding.empty() && has_gzip)
	{
		response.headers.resize(3);
		response.headers[2].name = "Content-Encoding";
		response.headers[2].value = "gzip";
	}
	response.status = static_cast<http_server::response::status_type>(code);
}

  void result_analysis(const double &time,std::string &stranswer) 
  {
	using rapidjson::Value;
	Value output_arry(rapidjson::kObjectType);
	Value item(rapidjson::kArrayType);
	Value timeObject(rapidjson::kNumberType);
	Value prob(rapidjson::kStringType); 
	Value cat(rapidjson::kStringType);
	Value faceid(rapidjson::kStringType);
	
	double dprob = 0;
	Value newprob(rapidjson::kNumberType);
	
	std::string result,str1,str2;
	char cbuffer[256];
	
	rapidjson::Document document,doc;
	document.Parse<0>(stranswer.c_str());
	rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
	
	Value bodyreply(rapidjson::kObjectType);
	//bodyreply = document["body"];
	if(document.IsObject())
	{
		if ( document.HasMember("body"))
			bodyreply = document["body"];
		else
		{
			return;
		}
	}
	else
	{
		return;
	}
	 
	if(!bodyreply.IsObject())
	{
		return;
	}
	Value& predictreply = bodyreply["predictions"];
	
	if (!predictreply.IsArray() || predictreply.Size() == 0)  
	{ 
		return;
	}
	bool flag_mulfacename = false;
	if( predictreply.Size() > 1 )
	{
		flag_mulfacename = true;
	}
	for (size_t i = 0; i < predictreply.Size(); ++i)  
	{  
		//Value item(rapidjson::kArrayType);
		Value& predictions = predictreply[i];
		assert(predictions.IsObject());
		Value& classesreply = predictions["classes"];
		
		//if (!classesreply.IsArray() || classesreply.Size() == 0)
		if (!classesreply.IsArray()) //no face output format is consistent 
		{
			return;
		}
		for (size_t j = 0; j < classesreply.Size(); ++j)  
		{	  
			Value singleface(rapidjson::kObjectType);	 
			Value & v = classesreply[j];  
			if( !v.IsObject() )
			{
				return;
			}  
			//faceid			
			if(flag_mulfacename)
			{
				sprintf(cbuffer,"%d-%d",(int)i,(int)j);
			}
			else
			{
				sprintf(cbuffer,"%d", (int)j); 
			}
			//sprintf(cbuffer,"%s", "");
			faceid.SetString(cbuffer,strlen(cbuffer),allocator);
			singleface.AddMember("face",faceid,allocator);
			
			
			//bbox
			Value bbox(rapidjson::kObjectType);
			if (!v.HasMember("bbox"))
			{  
				return;
			}
			if(v["bbox"].HasMember("xmin"))
			{
				if( v["bbox"]["xmin"].IsDouble())
				{
					//str1 = std::to_string(v["bbox"]["xmin"].GetDouble()).substr(0,5);
					//std::string str_value(str1);	
					//prob.SetString(str_value.c_str(),str_value.size(),allocator);
					//bbox.AddMember("xmin",prob,allocator);
					dprob = v["bbox"]["xmin"].GetDouble();
					newprob.SetDouble(dprob);
					bbox.AddMember("xmin",newprob,allocator);
					
				}
			} 
			if(v["bbox"].HasMember("ymin"))
			{
				if( v["bbox"]["ymin"].IsDouble())
				{
					dprob = v["bbox"]["ymin"].GetDouble();
					newprob.SetDouble(dprob);
					bbox.AddMember("ymin",newprob,allocator);
				}
			} 
			if(v["bbox"].HasMember("xmax"))
			{
				if( v["bbox"]["xmax"].IsDouble())
				{
					dprob = v["bbox"]["xmax"].GetDouble();
					newprob.SetDouble(dprob);
					bbox.AddMember("xmax",newprob,allocator);
				}
			} 
			if(v["bbox"].HasMember("ymax"))
			{
				if( v["bbox"]["ymax"].IsDouble())
				{
					dprob = v["bbox"]["ymax"].GetDouble();
					newprob.SetDouble(dprob);
					bbox.AddMember("ymax",newprob,allocator);
				}
			} 
			singleface.AddMember("bbox",bbox,allocator); 
			
			//emotion
			Value emotion(rapidjson::kObjectType);
			std::vector<std::string> emotioncat;
			if (v.HasMember("cat"))
			{
				if(v["cat"].IsString()) 
				{  
					str1 = v["cat"].GetString();
				}
			}
			if(string_analysis(str1,emotioncat) < 0)
			{
				return;
			}
			int emcat_size = (int)emotioncat.size();
			if(emcat_size<1)
			{
				return;
			}
            // 2017-11-23 calc attention prob modify st
            std::vector<double> emotionprob;
            // 2017-11-23 calc attention prob modify end
			for (int k = 0;k<emcat_size;k++)
			{
				if(v["bbox"].HasMember(emotioncat[k].c_str()))
				{
					if( v["bbox"][emotioncat[k].c_str()].IsDouble())
					{
						//str1 = std::to_string(v["bbox"][emotioncat[k].c_str()].GetDouble()).substr(0,5);
						//std::string str_value(str1);	
						//prob.SetString(str_value.c_str(),str_value.size(),allocator);
						dprob = v["bbox"][emotioncat[k].c_str()].GetDouble();
						newprob.SetDouble(dprob);
						cat.SetString(emotioncat[k].c_str(),emotioncat[k].size(),allocator);
						emotion.AddMember(cat,newprob,allocator);
                        
                        // 2017-11-23 calc attention prob modify st
                        emotionprob.push_back(dprob);
                        // 2017-11-23 calc attention prob modify end
					}
				}
			}
		  
			singleface.AddMember("emotion",emotion,allocator);
			
			// prob --> pad + svm
			if (v.HasMember("prob"))
			{
				if(v["prob"].IsDouble()) 
				{  
					//str2 = std::to_string(v["prob"].GetDouble()).substr(0,5);	 
					//std::string prob_str(str2);  
					//prob.SetString(prob_str.c_str(),prob_str.size(),allocator);
					
                    // 2017-11-23 calc attention prob modify st
                    calcattentionprob(emotioncat,emotionprob,dprob);
                    // 2017-11-23 calc attention prob modify end
                    
                    //dprob = v["prob"].GetDouble();
					newprob.SetDouble(dprob);
					singleface.AddMember("attention",newprob,allocator);
				}
			} 
			item.PushBack(singleface,allocator); 
		}		 
	}
	output_arry.AddMember("data",item,allocator);
	//time
	timeObject.SetDouble(time);
	output_arry.AddMember("time",timeObject,allocator);
	
	rapidjson::StringBuffer buffer;		 
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	output_arry.Accept(writer);
	stranswer = buffer.GetString();
	
	return; 
  }
  int string_analysis(const std::string &inputstr,std::vector<std::string> &classcat)
  {
	classcat.clear();
	int num = 0;
	std::vector<int>npos;
	npos.push_back(0);
	num++;
	size_t fi = inputstr.find(",", 0);
	if(fi!=inputstr.npos && 0 < fi && fi < (inputstr.length() - 1))//The comma	is not the start-0	and not the end-(length()-1)
	{
		npos.push_back(fi);
	}
	else
	{
		return -1;
	}
	while (fi!=inputstr.npos)
	{
		num++;
		fi = inputstr.find(",", fi + 1);
		if(fi!=inputstr.npos)
		{
			if( 0 < fi && fi < (inputstr.length() - 1) )  //The comma  is not the start-0  and not the end-(length()-1)
			{
				npos.push_back(fi);
			}
			else
			{
				return -2;
			}
		}
		
		if(num > 100)
		{
			return -5;
		}
	}
	
	if (num < 2)
	{
		return -3;
	}
	classcat.push_back(inputstr.substr(npos[0],npos[1]-npos[0]));
	for(size_t i = 1;i<(npos.size() - 1);i++)
	{
		if((npos[i+1]-npos[i]-1) < 1)//Two comma continuous 
		{
			return -4;
		}
		classcat.push_back(inputstr.substr(npos[i]+1,(npos[i+1]-npos[i]-1)));
			
	}
	classcat.push_back(inputstr.substr(npos[npos.size()-1]+1));
	
	return 0;
  }
int calcattentionprob(std::vector<std::string>emotioncat,std::vector<double>emotionprob,double &dprob)
  {
    dprob = 0;
    if(emotioncat.size() != emotionprob.size() || emotioncat.size()<1)
    {
        return -1;
    }
    double dP = 0.0;
    double dA = 0.0;
    double dD = 0.0;
    //int num = (int)emotioncat.size();
    
    //double dalpfa = 0.5393;
    //double dbeta = 0.3047;
    //double dgamma = -0.1190;
    //double dalpfa = 0.6084;
    //double dbeta = 0.3155;
    //double dgamma = -0.1700;
    //double dalpfa = 0.708;
    //double dbeta  = 0.3604;
    //double dgamma = -0.2063;
    //double ddelta = 0.6;
    
    double dcoffP      = 0.0;
    double dcoffA      = 0.0;
    double dcoffD      = 0.0;
    //double dsingleprob = 0.0;
    //double dsumprob    = 0.0;
   
    for (size_t i = 0;i<emotioncat.size();i++)
    {
        if ( emotioncat[i] == "Happy" )
        {
            dcoffP = 0.40;
            dcoffA = 0.20;
            dcoffD = 0.15;
        }
        if ( emotioncat[i] == "Angry" )
        {
            dcoffP = -0.51;
            dcoffA = 0.59;
            dcoffD = 0.25;
        }
        if ( emotioncat[i] == "Fear")
        {
            dcoffP = -0.64;
            dcoffA = 0.6;
            dcoffD = -0.43;
        }
        if ( emotioncat[i] == "Surprise")
        {
            dcoffP = 0.3;
            dcoffA = 0.6;
            dcoffD = -0.43;
        }
        if ( emotioncat[i] == "Sad" )
        {
            dcoffP = -0.4;
            dcoffA = -0.2;
            dcoffD = -0.5;
        }
        if ( emotioncat[i] == "Disgust" )
        {
            dcoffP = -0.4;
            dcoffA = 0.2;
            dcoffD = 0.1;
        }
        if ( emotioncat[i] == "Neutral" )
        {
            dcoffP = 0.0;
            dcoffA = 0.0;
            dcoffD = 0.0;
        }   
        
        dP += (emotionprob[i] * dcoffP);
        dA += (emotionprob[i] * dcoffA);
        dD += (emotionprob[i] * dcoffD);
        
    }
    //P_M = [0 0 0.21 0.59 0.19;0.15 0 0 0.3 -0.57;0.25 0.17 0.6 -0.32 0];
    //M = pinv(P_M)
    //M(5,:) = [0.6599 -1.4168 -0.0748]    
    //ocean = pinv(P_M) * [P A D]'
    
    double dneur = 0.6599*dP - 1.4168*dA - 0.0748*dD;
    //LOG(INFO)<<"dP = "<<dP;
    //LOG(INFO)<<"dA = "<<dA;
    //LOG(INFO)<<"dD = "<<dD;
    //LOG(INFO)<<"dneur = "<<dneur;
    dprob = 1/(1 + exp(-1.0*(dneur + 0.6)*3));
    double dagre = 1.2502*dP + 0.4374*dA- 0.3899*dD;
    double dprobagre = 1/(1 + exp(-1.0*(dagre)*3));
    dprob = (dprobagre + dprob)/2;
    //LOG(INFO)<<"dprob = "<<dprob;
    return 0;
  }
//2017-11-18 modify by lg end
  void operator()(http_server::request const &request,
		  http_server::response &response)
  {
	//debug
	/*std::cerr << "uri=" << request.destination << std::endl;
	std::cerr << "method=" << request.method << std::endl;
	std::cerr << "source=" << request.source << std::endl;
	std::cerr << "body=" << request.body << std::endl;*/
	//debug

	std::chrono::time_point<std::chrono::system_clock> tstart = std::chrono::system_clock::now();
	std::string access_log =  request.source + " \"" + request.method + " " + request.destination + "\"";
	int code;	 
	std::string source = request.source;
	if (source == "::1")
	  source = "127.0.0.1";
	uri::uri ur;
	ur << uri::scheme("http")
	   << uri::host(source)
	   << uri::path(request.destination);

	std::string req_method = request.method;
	std::string req_path = uri::path(ur);
	std::string req_query = uri::query(ur);
	std::transform(req_path.begin(),req_path.end(),req_path.begin(),::tolower);
	std::transform(req_query.begin(),req_query.end(),req_query.begin(),::tolower);
	std::vector<std::string> rscs = dd::dd_utils::split(req_path,'/');
	if (rscs.empty())
	  {
	LOG(ERROR) << "empty resource\n";
	response = http_server::response::stock_reply(http_server::response::not_found,_hja->jrender(_hja->dd_not_found_404()));
	return;
	  }
	std::string body = request.body;
	
	//debug
	/*std::cerr << "ur=" << ur << std::endl;
	std::cerr << "path=" << req_path << std::endl;
	std::cerr << "query=" << req_query << std::endl;
	std::cerr << "rscs size=" << rscs.size() << std::endl;
	std::cerr << "path1=" << rscs[1] << std::endl;
	LOG(INFO) << "HTTP " << req_method << " / call / uri=" << ur << std::endl;*/
	//debug

	std::string content_encoding;
	std::string accept_encoding;
	for (const auto& header : request.headers) {
	  if (header.name == "Accept-Encoding")
	  accept_encoding = header.value;
	  else if (header.name == "Content-Encoding")
	content_encoding = header.value;
	}
	bool encoding_error = false;
	if (!content_encoding.empty())
	  {
	if (content_encoding == "gzip")
	  {
		if (!body.empty())
		  {
		try
		  {
			std::string gzstr;
			filtering_ostream gzin;
			gzin.push(gzip_decompressor());
			gzin.push(boost::iostreams::back_inserter(gzstr));
			gzin << body;
			boost::iostreams::close(gzin);
			body = gzstr;
		  }
		catch(const std::exception &e)
		  {
			LOG(ERROR) << e.what() << std::endl;
			fillup_response(response,_hja->dd_bad_request_400(),access_log,code,tstart);
			code = 400;
			encoding_error = true;
		  }
		  }
	  }
	else
	  {
		LOG(ERROR) << "Unsupported content-encoding:" << content_encoding << std::endl;
		fillup_response(response,_hja->dd_bad_request_400(),access_log,code,tstart);
		code = 400;
		encoding_error = true;
	  }
	  }

	if (!encoding_error)
	{
		if (rscs.at(0) == _rsc_info)
		{
			fillup_response(response,_hja->info(),access_log,code,tstart,accept_encoding);
		}
		else if (rscs.at(0) == _rsc_services)
		{
			if (rscs.size() < 2)
			{
				fillup_response(response,_hja->dd_bad_request_400(),access_log,code,tstart);
				LOG(ERROR) << access_log << std::endl;
				return;
		  }
		std::string sname = rscs.at(1);
		if (req_method == "GET")
		  {
		fillup_response(response,_hja->service_status(sname),access_log,code,tstart,accept_encoding);
		  }
		else if (req_method == "PUT" || req_method == "POST") // tolerance to using POST
		  {
		fillup_response(response,_hja->service_create(sname,body),access_log,code,tstart,accept_encoding);
		  }
		else if (req_method == "DELETE")
		  {
		// DELETE does not accept body so query options are turned into JSON for internal processing
		std::string jstr = dd::uri_query_to_json(req_query);
		fillup_response(response,_hja->service_delete(sname,jstr),access_log,code,tstart,accept_encoding);
		  }
	  }
	else if (rscs.at(0) == _rsc_predict)
	  {
		if (req_method != "POST")
		  {
		fillup_response(response,_hja->dd_bad_request_400(),access_log,code,tstart);
		LOG(ERROR) << access_log << std::endl;
		return;
		  }
		//2017-11-18 modify by lg
		//fillup_response(response,_hja->service_predict(body),access_log,code,tstart,accept_encoding);
			fillup_response_branch(body,response,_hja->service_predict(body),access_log,code,tstart,accept_encoding);////////////////
	  }
	else if (rscs.at(0) == _rsc_train)
	  {
		if (req_method == "GET")
		  {
		std::string jstr = dd::uri_query_to_json(req_query);
		fillup_response(response,_hja->service_train_status(jstr),access_log,code,tstart,accept_encoding);
		  }
		else if (req_method == "PUT" || req_method == "POST")
		  {
		fillup_response(response,_hja->service_train(body),access_log,code,tstart,accept_encoding);
		  }
		else if (req_method == "DELETE")
		  {
		// DELETE does not accept body so query options are turned into JSON for internal processing
		std::string jstr = dd::uri_query_to_json(req_query);
		fillup_response(response,_hja->service_train_delete(jstr),access_log,code,tstart);
		  }
	  }
	else
	  {
		LOG(ERROR) << "Unknown Service=" << rscs.at(0) << std::endl;
		response = http_server::response::stock_reply(http_server::response::not_found,_hja->jrender(_hja->dd_not_found_404()));
		code = 404;
	  }
	  }
	std::time_t t = std::time(nullptr);
#if __GNUC__ >= 5
	if (code == 200 || code == 201)
	  LOG(INFO) << std::put_time(std::localtime(&t), "%c %Z") << " - " << access_log << std::endl;
	else LOG(ERROR) << std::put_time(std::localtime(&t), "%c %Z") << " - " << access_log << std::endl;
#else
	char mltime[128];
	strftime(mltime,sizeof(mltime),"%c %Z", std::localtime(&t));
	if (code == 200 || code == 201)
	  LOG(INFO) << mltime << " - " << access_log << std::endl;
	else LOG(ERROR) << mltime << " - " << access_log << std::endl;
#endif
  }
  void log(http_server::string_type const &info)
  {
	LOG(ERROR) << info << std::endl;
  }

  dd::HttpJsonAPI *_hja;
  std::string _rsc_info = "info";
  std::string _rsc_services = "services";
  std::string _rsc_predict = "predict";
  std::string _rsc_train = "train";
};

namespace dd
{
  volatile std::sig_atomic_t _sigstatus;

  /* variables for C-like signal handling */
  HttpJsonAPI *_ghja = nullptr;
  http_server *_gdd_server = nullptr;
  
  HttpJsonAPI::HttpJsonAPI()
	:JsonAPI()
  {
  }

  HttpJsonAPI::~HttpJsonAPI()
  {
	delete _dd_server;
  }

  int HttpJsonAPI::start_server(const std::string &host,
				const std::string &port,
				const int &nthreads)
  {
	APIHandler ahandler(this);
	http_server::options options(ahandler);
	_dd_server = new http_server(options.address(host)
				 .port(port)
				 .linger(false)
				 .reuse_address(true));
	_ghja = this;
	_gdd_server = _dd_server;
	LOG(INFO) << "Running DeepDetect HTTP server on " << host << ":" << port << std::endl;

	std::vector<std::thread> ts;
	for (int i=0;i<nthreads;i++)
	  ts.push_back(std::thread(std::bind(&http_server::run,_dd_server)));
	try {
	  _dd_server->run();
	}
	catch(std::exception &e)
	  {
	LOG(ERROR) << e.what() << std::endl;
	return 1;
	  }
	for (int i=0;i<nthreads;i++)
	  ts.at(i).join();
	return 0;
  }

  int HttpJsonAPI::start_server_daemon(const std::string &host,
					   const std::string &port,
					   const int &nthreads)
  {
	//_ft = std::async(&HttpJsonAPI::start_server,this,host,port,nthreads);
	std::thread t(&HttpJsonAPI::start_server,this,host,port,nthreads);
	t.detach();
	return 0;
  }
  
  void HttpJsonAPI::stop_server()
  {
	LOG(INFO) << "stopping HTTP server\n";
	if (_dd_server)
	  {
	try
	  {
		_dd_server->stop();
		_ft.wait();
		delete _dd_server;
		_gdd_server = nullptr;
	  }
	catch (std::exception &e)
	  {
		LOG(ERROR) << e.what() << std::endl;
	  }
	  }
  }

  void HttpJsonAPI::terminate(int param)
   {
	(void)param;
	if (_ghja)
	  {
	_ghja->stop_server();
	  }
   }
   
  int HttpJsonAPI::boot(int argc, char *argv[])
  {
	google::ParseCommandLineFlags(&argc, &argv, true);
	std::signal(SIGINT,terminate);
	return start_server(FLAGS_host,FLAGS_port,FLAGS_nthreads);
  }

}
