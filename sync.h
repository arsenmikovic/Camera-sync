/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2023, Raspberry Pi Ltd
 *
 * sync.h - sync algorithm
 */
#pragma once

#include <arpa/inet.h>
#include <netinet/ip.h>

#include "../sync_algorithm.h"

namespace RPiController {

struct SyncPayload {
	/* Wall clock time for this frame */
	int64_t wallClock;
	/* Capture sequence number */
	uint64_t sequence;

	uint64_t sensorTimestamp;
	
	/* Wall clock time for the next sync period */
	int64_t nextWallClock;
	/* Capture sequence number at the next sync period */
	uint64_t nextSequence;
	/* Ready signal */
	uint32_t readyFrame;
};

class Sync : public SyncAlgorithm
{
public:
	Sync(Controller *controller);
	~Sync();
	char const *name() const override;
	int read(const libcamera::YamlObject &params) override;
	void initialise() override;
	void switchMode(CameraMode const &cameraMode, Metadata *metadata) override;
	void process(StatisticsPtr &stats, Metadata *imageMetadata) override;
	void setFrameDuration(libcamera::utils::Duration frameDuration) override;

private:

	enum class State { Idle, Correcting, Stabilising };

	State state_;
	Mode mode_;
	std::string group_;
	uint16_t port_;
	uint32_t syncPeriod_;
	uint32_t readyFrame_;
	uint32_t sufficientSync_;
	std::string usingWallClock_;

	struct sockaddr_in addr_;
	int socket_;
	libcamera::utils::Duration frameDuration_;
	unsigned int frameCount_;
	unsigned int syncMark_;
	SyncPayload lastPayload_;
	unsigned int readyCountdown_;
	unsigned int syncCount_;
	bool syncReady_;
};

} /* namespace RPiController */
