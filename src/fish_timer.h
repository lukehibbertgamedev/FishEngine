
#pragma once

// GameTimer with thanks to https://terrorgum.com/tfox/books/introductionto3dgameprogrammingwithdirectx12.pdf

namespace Fish {

	namespace Timer {

		class EngineTimer {
		public:
			EngineTimer();

			float engine_time() const; // in secs
			float delta_time() const; // in secs
			float frame_time() const; // in msecs

			void reset(); // before msg loop
			void start(); // unpaused
			void stop(); // paused
			void tick(); // per frame
		private:

			double mSecondsPerCount;
			double mDeltaTime;
			__int64 mBaseTime;
			__int64 mPausedTime;
			__int64 mStopTime;
			__int64 mPrevTime;
			__int64 mCurrTime;
			bool mStopped;
		};

	}
}
