#include "sync.h"

#include <cctype>
#include <chrono>
#include <fcntl.h>
#include <map>
#include <strings.h>
#include <unistd.h>
#include <vector>
#include<list>

#include <libcamera/base/log.h>

#include "sync_status.h"
#include "clock_recovery.h"

using namespace std;
using namespace std::chrono_literals;
using namespace RPiController;
using namespace libcamera;

LOG_DEFINE_CATEGORY(RPiSync)

#define NAME "rpi.sync"

const char *DefaultGroup = "239.255.255.250";
constexpr unsigned int DefaultPort = 10000;
constexpr unsigned int DefaultSyncPeriod = 30;
constexpr unsigned int DefaultReadyFrame = 1000;
const char *DefaultUsingWallClock = "no";

Sync::Sync(Controller *controller)
	: SyncAlgorithm(controller), mode_(Mode::Off), socket_(-1), frameDuration_(0s), frameCount_(0)
{
}

Sync::~Sync()
{
	if (socket_ >= 0)
		close(socket_);
}

char const *Sync::name() const
{
	return NAME;
}
/* This reads from json file and intitiaises server and client */
int Sync::read(const libcamera::YamlObject &params)
{
	static const std::map<std::string, Mode> modeMapping = {
		{ "off", Mode::Off },
		{ "client", Mode::Client },
		{ "server", Mode::Server },
	};

	std::string mode = params["mode"].get<std::string>("off");
	std::transform(mode.begin(), mode.end(), mode.begin(),
		       [](unsigned char c) { return std::tolower(c); });

	auto it = modeMapping.find(mode);
	if (it == modeMapping.end()) {
		LOG(RPiSync, Error) << "Invalid mode specificed: " << mode;
		return -EINVAL;
	}
	mode_ = it->second;

	group_ = params["group"].get<std::string>(DefaultGroup);
	port_ = params["port"].get<uint16_t>(DefaultPort);
	syncPeriod_ = params["sync_period"].get<uint32_t>(DefaultSyncPeriod);
	readyFrame_ = params["ready_frame"].get<uint32_t>(DefaultReadyFrame);
	usingWallClock_ = params["using_wall_clock"].get<std::string>(DefaultUsingWallClock);

	return 0;
}
/* Setting up slient camera to receive what server sends */
void Sync::initialise()
{
	socket_ = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_ < 0) {
		LOG(RPiSync, Error) << "Unable to create server socket.";
		return;
	}

	memset(&addr_, 0, sizeof(addr_));
        addr_.sin_family = AF_INET;
        addr_.sin_addr.s_addr = mode_ == Mode::Client ? htonl(INADDR_ANY) : inet_addr(group_.c_str());
        addr_.sin_port = htons(port_);

	if (mode_ == Mode::Client) {
		/* Set to non-blocking. */
		int flags = fcntl(socket_, F_GETFL, 0);
		fcntl(socket_, F_SETFL, flags | O_NONBLOCK);

		unsigned int en = 1;
		if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(en)) < 0) {
			LOG(RPiSync, Error) << "Unable to set socket options";
			goto err;
		}

                struct ip_mreq mreq {};
		mreq.imr_multiaddr.s_addr = inet_addr(group_.c_str());
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
		if (setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
			LOG(RPiSync, Error) << "Unable to set socket options";
			goto err;
		}

		if (bind(socket_, (struct sockaddr *) &addr_, sizeof(addr_)) < 0) {
			LOG(RPiSync, Error) << "Unable to bind client socket.";
			goto err;
		}
	}

	return;

err:
	close(socket_);
	socket_ = -1;
}

/* No idea what this does*/
void Sync::switchMode([[maybe_unused]] CameraMode const &cameraMode, [[maybe_unused]] Metadata *metadata)
{
	syncReady_ = false;
	frameCount_ = 0;
	readyCountdown_ = 0;
}


/* Most important part, algorithm*/
void Sync::process([[maybe_unused]] StatisticsPtr &stats, Metadata *imageMetadata)
{
	SyncPayload payload;
	SyncParams local {};
	SyncStatus status {};
	imageMetadata->get("sync.params", local);


	if (!frameDuration_) {
		LOG(RPiSync, Error) << "Sync frame duration not set!";
		return;
	}

	if (mode_ == Mode::Server) {

		static ClockRecovery trendingClock(local.wallClock, local.sensorTimestamp, syncPeriod_, 100); 
		//LOG(RPiSync, Error) << "wall clock  "<<local.wallClock<<" smth like ready frame "<<readyFrame_ - frameCount_<<" sent "<<local.sensorTimestamp;
		if (!syncReady_ && !(readyFrame_ - frameCount_)) {
			LOG(RPiSync, Error) << "Wall clock "<<  local.wallClock<< " the other thing "<<readyFrame_ - frameCount_ ;
			syncReady_ = true;
			if(usingWallClock_ == "yes"){
					LOG(RPiSync, Error) << "using trending wall clock";
					LOG(RPiSync, Error) << "Wall clock is "<< local.wallClock;
				}
		}

		if (!(frameCount_ % syncPeriod_)) {
			static int64_t nextSensorTimestamp = local.wallClock;
			payload.sequence = local.sequence;
			payload.wallClock = trendingClock.modeled_wall_clock(local.wallClock, local.sensorTimestamp, local.sequence);
			payload.sensorTimestamp = local.sensorTimestamp;
			payload.nextSequence = local.sequence + syncPeriod_;
			payload.nextWallClock = payload.wallClock + frameDuration_.get<std::micro>() * syncPeriod_;
			payload.readyFrame = std::max<int32_t>(0, readyFrame_ - frameCount_);

			//jitter without sensor timestmp will wall clock, sine nt big eal with sensor timestamp
			int64_t jitter = payload.wallClock - nextSensorTimestamp;
			nextSensorTimestamp = payload.nextWallClock;

			if (sendto(socket_, &payload, sizeof(payload), 0, (const sockaddr *)&addr_, sizeof(addr_)) < 0){
				LOG(RPiSync, Error) << "Send error! "<< strerror(errno);
			} else{
				LOG(RPiSync, Info) << "Sent message: seq " << payload.sequence <<" jitter " << jitter << "us" << " : ready frame " << payload.readyFrame;
			}
		}
	} else if (mode_ == Mode::Client) {

		static ClockRecovery trendig_error;
		static ClockRecovery trendingClock(local.wallClock, local.sensorTimestamp, syncPeriod_, 100);
		static int frames = 0;
		socklen_t addrlen = sizeof(addr_);

		/* Approximates frame duration */
		static int64_t modeled_wall_clock_value = 0;
		static int64_t last_wall_clock_value = 0;
		static std::chrono::microseconds delta_mod = 0us;
		static std::chrono::microseconds lastPayloadFrameDuration = 0us;
		static std::chrono::microseconds expected = 0us;
		

		while (true) {
			int ret = recvfrom(socket_, &lastPayload_, sizeof(lastPayload_), 0, (struct sockaddr *)&addr_, &addrlen);

			if (ret > 0) {
				if(!syncReady_){
					state_ = State::Correcting;
					//LOG(RPiSync, Error) << "Received";
				}
				frames = 0;	
	
				if(usingWallClock_ == "yes"){
					//modeled_wall_clock_value = local.wallClock;
					modeled_wall_clock_value = trendingClock.modeled_wall_clock(local.wallClock, local.sensorTimestamp, local.sequence);
					last_wall_clock_value = lastPayload_.wallClock;
				} else{
					modeled_wall_clock_value = (local.sensorTimestamp)/1000;
					last_wall_clock_value = (lastPayload_.sensorTimestamp)/1000;
				}

				lastPayloadFrameDuration = (lastPayload_.nextWallClock - lastPayload_.wallClock) * 1us / (lastPayload_.nextSequence - lastPayload_.sequence);
				std::chrono::microseconds delta = (modeled_wall_clock_value * 1us) - (last_wall_clock_value* 1us);
				unsigned int mul = (delta + lastPayloadFrameDuration / 2) / lastPayloadFrameDuration;
				delta_mod = delta - mul * lastPayloadFrameDuration;				

				if (!syncReady_)
					readyCountdown_ = lastPayload_.readyFrame + frameCount_;
				
				if (lastPayload_.readyFrame <= syncPeriod_ && !syncReady_ && lastPayload_.readyFrame){
					expected = lastPayload_.wallClock *1us + lastPayload_.readyFrame * lastPayloadFrameDuration;
					LOG(RPiSync, Info) << "Expected sync  "<<expected;
				}
				} else
					break;
		}
		//LOG(RPiSync, Error) << "wall clock  "<<local.wallClock<<" smth like ready frame "<<readyCountdown_ - frameCount_<<" sent "<<local.sensorTimestamp;
		if(syncReady_ && !frames){
			delta_mod = trendig_error.trending_error(last_wall_clock_value * 1us, modeled_wall_clock_value * 1us, lastPayloadFrameDuration, local.sequence);
			if(abs(delta_mod) > 50us){
				trendig_error.updating_values(delta_mod);
				state_ = State::Correcting;
			}
		}

		if (state_ == State::Correcting)  {
			LOG(RPiSync, Info) << "Correcting by "<< delta_mod;
			status.frameDurationOffset = delta_mod;
			state_ = State::Stabilising;
		} else if (state_ == State::Stabilising) {
			status.frameDurationOffset = 0s;		
			state_ = State::Idle;
		}
		//LOG(RPiSync, Error) << "Wall clock is "<< local.wallClock<< " frame countor smth like that "<< readyCountdown_ - frameCount_;

		if (!syncReady_ && readyCountdown_ && local.wallClock * 1us < expected + lastPayloadFrameDuration/2 && local.wallClock *1us > expected - lastPayloadFrameDuration/2) {
			//LOG(RPiSync, Error) << "Wall clock is "<< local.wallClock<< " frame countor smth like that "<< readyCountdown_ - frameCount_;
			syncReady_ = true;
			LOG(RPiSync, Error) << "Sync ready is true";
			LOG(RPiSync, Error) << "Wall clock is at sync is"<< local.wallClock;
			trendingClock.clear();
		}
		frames++;
	}

	status.ready = syncReady_;
	imageMetadata->set("sync.status", status);
	frameCount_++;
}

void Sync::setFrameDuration(libcamera::utils::Duration frameDuration)
{
	frameDuration_ = frameDuration;
};

/* Register algorithm with the system. */
static Algorithm *create(Controller *controller)
{
	return (Algorithm *)new Sync(controller);
}
static RegisterAlgorithm reg(NAME, &create);