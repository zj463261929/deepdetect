/**
 * DeepDetect
 * Copyright (c) 2014 Emmanuel Benazera
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

#ifndef CAFFEMODEL_H
#define CAFFEMODEL_H

#include "mlmodel.h"
#include "apidata.h"
#include <string>

namespace dd
{
	class CaffeModel : public MLModel
	{
	public:
		CaffeModel():MLModel() {}
		CaffeModel(const APIData &ad, int nIndex=0); //2017-11-10 modify by zj
		CaffeModel(const std::string &repo)
		  :MLModel(repo) {}
		~CaffeModel() {};

		int read_from_repository(const std::string &repo);
		int read_from_repository_mtcnn(const std::string &repo);
		
		std::string _def; /**< file name of the model definition in the form of a protocol buffer message description. */
		std::string _trainf; /**< file name of the training model definition. */
		std::string _weights; /**< file name of the network's weights. */
		std::string _solver; /**< solver description file, included here as part of the model, very specific to Caffe. */
		std::string _sstate; /**< current solver state, useful for resuming training. */
		std::string _model_template; /**< model template name, if any. */
		bool _has_mean_file = false; /**< whether a mean.binaryproto file is available, for image models only. */
		
		std::string _def1, _def2, _def3;
		std::string _weights1, _weights2, _weights3;
		std::string sModelPath;
	};
}

#endif
