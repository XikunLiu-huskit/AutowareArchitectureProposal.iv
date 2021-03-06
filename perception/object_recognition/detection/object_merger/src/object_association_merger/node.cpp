/*
 * Copyright 2020 Tier IV, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud2_iterator.h>
#include <tf2/LinearMath/Transform.h>
#include <tf2/convert.h>
#include <tf2/transform_datatypes.h>
#include <tf2_sensor_msgs/tf2_sensor_msgs.h>
#include <chrono>
#include <object_association_merger/node.hpp>
#define EIGEN_MPL2_ONLY
#include <Eigen/Core>
#include <Eigen/Geometry>

namespace object_association
{
ObjectAssociationMergerNode::ObjectAssociationMergerNode()
: nh_(""),
  pnh_("~"),
  tf_listener_(tf_buffer_),
  object0_sub_(pnh_, "input/object0", 1),
  object1_sub_(pnh_, "input/object1", 1),
  sync_(SyncPolicy(10), object0_sub_, object1_sub_)
{
  sync_.registerCallback(boost::bind(&ObjectAssociationMergerNode::objectsCallback, this, _1, _2));

  merged_object_pub_ =
    pnh_.advertise<autoware_perception_msgs::DynamicObjectWithFeatureArray>("output/object", 10);
}

void ObjectAssociationMergerNode::objectsCallback(
  const autoware_perception_msgs::DynamicObjectWithFeatureArray::ConstPtr & input_object0_msg,
  const autoware_perception_msgs::DynamicObjectWithFeatureArray::ConstPtr & input_object1_msg)
{
  // Guard
  if (merged_object_pub_.getNumSubscribers() < 1) return;

  // build output msg
  autoware_perception_msgs::DynamicObjectWithFeatureArray output_msg;
  output_msg.header = input_object0_msg->header;

  /* global nearest neighbor */
  std::unordered_map<int, int> direct_assignment;
  std::unordered_map<int, int> reverse_assignment;
  Eigen::MatrixXd score_matrix =
    data_association_.calcScoreMatrix(*input_object1_msg, *input_object0_msg);
  data_association_.assign(score_matrix, direct_assignment, reverse_assignment);
  for (size_t object0_idx = 0; object0_idx < input_object0_msg->feature_objects.size();
       ++object0_idx) {
    if (direct_assignment.find(object0_idx) != direct_assignment.end())  // found
    {
      // The one with the higher score will be hired.
      if (
        input_object1_msg->feature_objects.at(direct_assignment.at(object0_idx))
          .object.semantic.confidence <
        input_object0_msg->feature_objects.at(object0_idx).object.semantic.confidence) {
        output_msg.feature_objects.push_back(input_object0_msg->feature_objects.at(object0_idx));
      } else {
        output_msg.feature_objects.push_back(
          input_object1_msg->feature_objects.at(direct_assignment.at(object0_idx)));
      }
    } else  // not found
    {
      output_msg.feature_objects.push_back(input_object0_msg->feature_objects.at(object0_idx));
    }
  }
  for (size_t object1_idx = 0; object1_idx < input_object1_msg->feature_objects.size();
       ++object1_idx) {
    if (reverse_assignment.find(object1_idx) != reverse_assignment.end())  // found
    {
    } else  // not found
    {
      output_msg.feature_objects.push_back(input_object1_msg->feature_objects.at(object1_idx));
    }
  }

  // publish output msg
  merged_object_pub_.publish(output_msg);
}
}  // namespace object_association
