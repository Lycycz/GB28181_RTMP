// leveldb µÄ Mutex CondVar
#pragma once
#include <mutex>
#include <condition_variable>
#include <cassert>

class Mutex {
public:
	Mutex() = default;
	~Mutex() = default;

	Mutex(const Mutex&) = delete;
	Mutex& operator=(const Mutex&) = delete;

	void Lock() { mu_.lock(); }
	void Unlock() { mu_.unlock(); }
	void AssertHeld() { }

private:
	friend class CondVar;
	std::mutex mu_;
};

class CondVar {
public:
	explicit CondVar(Mutex* mu) : mu_(mu) { assert(mu != nullptr); }
	~CondVar() = default;

	CondVar(const CondVar&) = delete;
	CondVar& operator=(const CondVar&) = delete;

	void Wait() {
		std::unique_lock<std::mutex> lock(mu_->mu_, std::adopt_lock);
		cv_.wait(lock);
		lock.release();
	}
	void Signal() { cv_.notify_one(); }
	void SignalAll() { cv_.notify_all(); }
private:
	std::condition_variable cv_;
	Mutex* const mu_;
};
