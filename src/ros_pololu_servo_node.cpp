#include <ros/ros.h>
#include <ros_pololu_servo/servo_pololu.h>
#include <ros_pololu_servo/pololu_state.h>
#include "PolstroSerialInterface.h"

const unsigned int baudRate = 38400;
const float pi = 3.141592653589793f;
const unsigned int channelMinValue = 3280;
const unsigned int channelMaxValue = 8700;
const unsigned int channelValueRange = channelMaxValue - channelMinValue;
const unsigned int signalPeriodInMs = 2000;
Polstro::SerialInterface* serialInterface;
std::string portName = "/dev/ttyACM0";
ros_pololu_servo::servo_pololu msgTemp,msgs;

std::string errstrs[] = {
	"Serial Signal Error (bit 0)",
	"Serial Overrun Error (bit 1)",
	"Serial RX buffer full (bit 2)",
	"Serial CRC error (bit 3)",
	"Serial protocol error (bit 4)",
	"Serial timeout error (bit 5)",
	"Script stack error (bit 6)",
	"Script call stack error (bit 7)",
	"Script program counter error (bit 8)"
};

bool status(ros_pololu_servo::pololu_state::Request  &req, 
ros_pololu_servo::pololu_state::Response &res)
{
	//
	unsigned char channelNumber=req.qid;
	bool ret;
	unsigned short position;
	res.id;
	serialInterface->getPositionCP( channelNumber, position );
	res.angle=(((float)(position-channelMinValue)/(float)channelValueRange)-0.5)*pi;
	ROS_INFO("getPositionCP(%d) (ret=%d position=%d)\n", channelNumber, ret, position );
	return true;
}


void CommandCallback(const ros_pololu_servo::servo_pololu::ConstPtr& msg)
{
	//ROS_INFO("I heard: [%s]", msg->data.c_str());
	//msgTemp=(*msg);
	unsigned char channelNumber=msg->id;
	if (msg->speed>0)serialInterface->setSpeedCP( channelNumber, msg->speed );
	if (msg->acceleration>0)serialInterface->setAccelerationCP( channelNumber, msg->acceleration );
	if ((msg->angle>=-pi/2)&&(msg->angle<=pi/2)){
		//
		int i=((msg->angle+pi/2.0)/pi)*(float)channelValueRange+(float)channelMinValue;
		ROS_INFO("motor %d : position=%d",channelNumber,i);
		if (i>=channelMinValue && i<=channelMaxValue)
		serialInterface->setTargetCP( channelNumber, i );
	}
}

bool handle_errors()
{
	unsigned short errors;
	if (!serialInterface->getErrorsCP(errors))
		return false;

	for (int i = 0; i < 8; ++i)
	{
		if (errors & 1) {
			ROS_ERROR("%s", errstrs[i].c_str());
		}
		errors >>= 1;
	}
	return true;
}

int main(int argc,char**argv)
{
	ros::init(argc, argv, "pololu_servo");
	ros::NodeHandle n;
	ROS_INFO("Creating serial interface '%s' at %d bauds\n", portName.c_str(), baudRate);
	serialInterface = Polstro::SerialInterface::createSerialInterface( portName, baudRate );
	if ( !serialInterface->isOpen() )
	{
		ROS_ERROR("Failed to open interface\n");
		return -1;
	}
	ros::Subscriber sub = n.subscribe("/cmd_pololu", 20, CommandCallback);
	ros::ServiceServer service = n.advertiseService("pololu_status", status);
	handle_errors();
    ROS_INFO("Ready...");

    int hz = 100;
    ros::Rate r(hz);
    int i = 0;
	while (ros::ok())
	{	
		//Limit error clearing to once a second.
		if (++i > hz) {
			i = 0;
			if (!handle_errors()) {
				ROS_INFO("Lost connection to the device.");
				//break;
			}
		}

		ros::spinOnce();
		r.sleep();
	}

	ROS_INFO("Deleting serial interface...");
	delete serialInterface;
	serialInterface = NULL;
	return 0;
}
