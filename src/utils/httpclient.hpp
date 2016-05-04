/**
 * DeepDetect
 * Copyright (c) 2016 Emmanuel Benazera
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with deepdetect.  If not, see <http://www.gnu.org/licenses/>.
 */

#define BOOST_NETWORK_ENABLE_HTTPS
#include <boost/network/include/http/client.hpp>

#ifndef DD_HTTPCLIENT_H
#define DD_HTTPCLIENT_H

namespace dd
{

  class httpclient
  {
  public:
    static void get_call(const std::string &url,
			 const std::string &http_method,
			 int &outcode,
			 std::string &outstr)
    {
      boost::network::http::client client;
      try {
	boost::network::http::client::request request(url);
	boost::network::http::client::response response;
	if (http_method == "GET")
	  response = client.get(request);
	else if (http_method == "DELETE")
	  response = client.delete_(request);
	outstr = response.body();
	outcode = response.status();
      } catch (std::exception& e) {
	// deal with exceptions here
	std::cerr << "http client exception\n";
      }
    }

    static void post_call(const std::string &url,
			  const std::string &jcontent,
			  const std::string &http_method,
			  int &outcode,
			  std::string &outstr,
			  const std::string &content_type="Content-Type: application/json")
    {
      boost::network::http::client client;
      try
	{
	  boost::network::http::client::request request(url);
	  request << boost::network::header("Content-Type", content_type);
	  boost::network::http::client::response response;
	  if (http_method == "PUT")
	    response = client.put(request,jcontent);
	  else if (http_method == "POST")
	    response = client.post(request,jcontent);
	  outstr = response.body();
	  outcode = response.status();
	}
      catch (std::exception &e)
	{
	  std::cerr << "http client post exception\n";
	}
    }
    
  };
  
}

#endif
