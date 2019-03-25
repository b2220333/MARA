#include "hros_cognition_mara_components_real/HROSCognitionMaraComponentsReal.hpp"

HROSCognitionMaraComponentsRealNode::HROSCognitionMaraComponentsRealNode(const std::string & node_name,
                   int argc, char **argv, bool intra_process_comms )
: rclcpp::Node(node_name, "", intra_process_comms)
{

  this->node_name = node_name;

  auto qos_state = rmw_qos_profile_sensor_data;
  qos_state.depth = 1;
  common_joints_pub_ = create_publisher<control_msgs::msg::JointTrajectoryControllerState>(
    		 "/mara_controller/state",
         qos_state);

  trajectory_sub_ = create_subscription<trajectory_msgs::msg::JointTrajectory>(
         "/mara_controller/command",
         std::bind(&HROSCognitionMaraComponentsRealNode::commandCallback, this, _1),
         rmw_qos_profile_sensor_data);

  if (rcutils_cli_option_exist(argv, argv + argc, "-motors")){
    file_motors = std::string(rcutils_cli_get_option(argv, argv + argc, "-motors"));
  }

  pthread_mutex_init(&mtx, NULL);
  pthread_mutex_init(&mutex_command, NULL);

  nan = std::numeric_limits<float>::quiet_NaN();

  RCUTILS_LOG_INFO_NAMED(get_name(), "HROSCognitionMaraComponentsRealNode::on_configure() is called.");

	std::vector<std::string> node_names;

  std::cout << "===================== Reading link order ========================" << std::endl;
  std::vector<std::string> topic_order;
  std::cout << "Trying to open  " << file_motors << std::endl;

  YAML::Node config = YAML::LoadFile(file_motors);
  if(config.IsNull()){
      std::cout << "Not able to open the file" << std::endl;
      return;
  }
  std::vector<std::string> lista_subcribers;

  for (auto motor : config["motors"]) {
    std::string s = motor.as<std::string>();
    topic_order.push_back(s);
    std::cout << "topic name: " << s << std::endl;
  }
  std::cout << "=====================================================" << std::endl;

  std::cout << "++++++++++++++++++ Subscribers and Publishers++++++++++++++++++" << std::endl;
  for(unsigned int i = 0; i < topic_order.size(); i++){

    std::string topic = topic_order[i];
    std::string delimiter = ":";
    std::string id = topic.substr( 0, topic.find(delimiter) );
    std::string axis = topic.erase( 0, topic.find(delimiter) + delimiter.length() );
    std::string motor_name = id + "/state_" + axis;

    auto subscriber = this->create_subscription<hrim_actuator_rotaryservo_msgs::msg::StateRotaryServo>(
                              std::string("/") + motor_name,
                              [this, motor_name](hrim_actuator_rotaryservo_msgs::msg::StateRotaryServo::UniquePtr msg) {
                                stateCallback(motor_name, msg->velocity, msg->position, msg->effort);
                              },rmw_qos_profile_sensor_data);
    motor_state_subcriptions_.push_back(subscriber);
    std::cout << "Subscribe at " << motor_name << std::endl;

    motor_name = id + "/goal_" + axis;

    auto publisher_command = this->create_publisher<hrim_actuator_rotaryservo_msgs::msg::GoalRotaryServo>(
                                   std::string("/") + motor_name,
                                   rmw_qos_profile_sensor_data);
    motor_goal_publishers_.push_back(publisher_command);
    std::cout << "New publisher at " << motor_name << std::endl;
  }

  std::cout << "+++++++++++++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
  std::cout << std::endl << std::endl;

  msg_actuators_.actual.positions.resize(motor_goal_publishers_.size());
  msg_actuators_.actual.velocities.resize(motor_goal_publishers_.size());
  msg_actuators_.actual.effort.resize(motor_goal_publishers_.size());
  msg_actuators_.joint_names.resize(motor_goal_publishers_.size());

  for(unsigned int i = 0; i < topic_order.size(); i++){

    std::string topic = topic_order[i];
    std::string delimiter = ":";
    std::string id = topic.substr( 0, topic.find(delimiter) );
    std::string axis = topic.erase( 0, topic.find(delimiter) + delimiter.length() );
    std::string motor_name = id + "/state_" + axis;

    msg_actuators_.joint_names[i] =  motor_name;
  }

  timer_common_joints_ = this->create_wall_timer(
      20ms, std::bind(&HROSCognitionMaraComponentsRealNode::timer_stateCommonPublisher, this));
  timer_command_ = this->create_wall_timer(
      10ms, std::bind(&HROSCognitionMaraComponentsRealNode::timer_commandPublisher, this));

  RCUTILS_LOG_INFO_NAMED(get_name(), "HROSCognitionMaraComponentsRealNode::on_configure() is finished.");
}
