/**
 * Seek and manual-mode tests for PressureController against a simulated tire.
 *
 * The rig models what makes seeking hard on real hardware: the sensor reads hose pressure
 * (offset while air flows), readings are noisy, and vent rate falls with pressure.
 * Tune ControllerConfig here first, then on a real tire.
 */

#include <gtest/gtest.h>
#include <math.h>
#include <TA_Controller.h>

using namespace trailair;
using namespace trailair::controller;
using trailair::errors::ErrorCode;

namespace {

struct FakeRig : IActuatorOutputs {
  float tire = 20.0f;          // true tire pressure
  bool compressor = false;
  bool vent = false;
  float inflateRate = 0.3f;    // PSI/s
  float ventFraction = 0.04f;  // vent rate = ventFraction * pressure, PSI/s
  float hoseOffset = 1.0f;     // sensor error while air flows (+ inflating, - venting)
  uint32_t seed = 12345;

  void setCompressor(bool on) override { compressor = on; if (on) vent = false; }
  void setVent(bool open) override { vent = open; if (open) compressor = false; }
  void stopAll() override { compressor = vent = false; }

  void step(float dt) {
    if (compressor) tire += inflateRate * dt;
    if (vent) tire -= ventFraction * tire * dt;
  }
  float noise() {  // deterministic, +/-0.1 PSI (roughly the filtered sensor)
    seed = seed * 1103515245u + 12345u;
    return ((seed >> 16) % 2001) / 10000.0f - 0.1f;
  }
  float reading() {
    float offset = compressor ? hoseOffset : (vent ? -hoseOffset : 0.0f);
    return tire + offset + noise();
  }
};

struct Sim {
  FakeRig rig;
  PressureController ctl;
  uint32_t now = 1000;

  explicit Sim(float tire, const ControllerConfig& cfg = ControllerConfig{}) {
    rig.tire = tire;
    ctl.begin(&rig, cfg);
    ctl.update(now, rig.reading());
  }

  void tick() {
    now += 25;  // ~ the board's loop period
    rig.step(0.025f);
    ctl.update(now, rig.reading());
  }

  /// Start a seek and run until it finishes (Idle) or errors. False on timeout.
  bool seek(float target, uint32_t limitMs) {
    ctl.startSeek(target, now);
    uint32_t end = now + limitMs;
    while (now < end) {
      tick();
      ControllerState s = ctl.getState();
      if (s == ControllerState::Idle || s == ControllerState::Error) return true;
    }
    return false;
  }
};

}  // namespace

// ============================================================================
// Seek
// ============================================================================

TEST(Seek, InflatesToTarget) {
  Sim sim(20.0f);
  ASSERT_TRUE(sim.seek(32.0f, 300000));
  EXPECT_EQ(sim.ctl.getState(), ControllerState::Idle);
  EXPECT_NEAR(sim.rig.tire, 32.0f, 0.5f);
  EXPECT_FALSE(sim.rig.compressor || sim.rig.vent);
}

TEST(Seek, DeflatesToTarget) {
  Sim sim(35.0f);
  ASSERT_TRUE(sim.seek(18.0f, 300000));
  EXPECT_EQ(sim.ctl.getState(), ControllerState::Idle);
  EXPECT_NEAR(sim.rig.tire, 18.0f, 0.5f);
}

TEST(Seek, FastCompressorConverges) {
  Sim sim(10.0f);
  sim.rig.inflateRate = 2.5f;  // small tire, big compressor: faster than maximumExpectedRate
  ASSERT_TRUE(sim.seek(30.0f, 300000));
  EXPECT_EQ(sim.ctl.getState(), ControllerState::Idle);
  EXPECT_NEAR(sim.rig.tire, 30.0f, 0.5f);
}

TEST(Seek, AlreadyAtTargetFinishesImmediately) {
  Sim sim(32.05f);
  sim.ctl.startSeek(32.0f, sim.now);
  EXPECT_EQ(sim.ctl.getState(), ControllerState::Idle);
  EXPECT_FALSE(sim.rig.compressor || sim.rig.vent);
}

TEST(Seek, SmallSlowCorrectionIsNotAStall) {
  Sim sim(31.5f);
  sim.rig.inflateRate = 0.05f;  // short probes near target move less than the noise floor
  ASSERT_TRUE(sim.seek(32.0f, 120000));
  EXPECT_EQ(sim.ctl.getState(), ControllerState::Idle);
}

TEST(Seek, DeadCompressorRaisesNoChange) {
  Sim sim(20.0f);
  sim.rig.inflateRate = 0.0f;
  ASSERT_TRUE(sim.seek(32.0f, 120000));
  EXPECT_EQ(sim.ctl.getState(), ControllerState::Error);
  EXPECT_EQ(sim.ctl.getError(), ErrorCode::NoChange);
  EXPECT_FALSE(sim.rig.compressor);
}

TEST(Seek, CompressorDyingMidSeekRaisesNoChange) {
  Sim sim(10.0f);
  sim.ctl.startSeek(40.0f, sim.now);
  for (int i = 0; i < 20000 / 25; ++i) sim.tick();  // learn the rate first
  sim.rig.inflateRate = 0.0f;
  for (int i = 0; i < 120000 / 25 && sim.ctl.getState() != ControllerState::Error; ++i) sim.tick();
  EXPECT_EQ(sim.ctl.getError(), ErrorCode::NoChange);
}

TEST(Seek, TargetClampedToLimits) {
  Sim sim(20.0f);
  sim.ctl.startSeek(80.0f, sim.now);
  EXPECT_FLOAT_EQ(sim.ctl.getTargetPSI(), sim.ctl.getConfig().maximumPSI);
  sim.ctl.startSeek(1.0f, sim.now);
  EXPECT_FLOAT_EQ(sim.ctl.getTargetPSI(), sim.ctl.getConfig().minimumPSI);
}

// ============================================================================
// Manual
// ============================================================================

TEST(Manual, LeaseExpiresWithoutRenewal) {
  Sim sim(30.0f);
  uint32_t start = sim.now;
  sim.ctl.manualAirUp(true, start);
  sim.ctl.update(start + 999, sim.rig.reading());
  EXPECT_EQ(sim.ctl.getState(), ControllerState::AirUp);
  sim.ctl.update(start + 1000, sim.rig.reading());
  EXPECT_EQ(sim.ctl.getState(), ControllerState::Idle);
  EXPECT_FALSE(sim.rig.compressor);
}

TEST(Manual, RenewalKeepsRunning) {
  Sim sim(30.0f);
  uint32_t start = sim.now;
  sim.ctl.manualVent(true, start);
  sim.ctl.manualVent(true, start + 800);  // e.g. remote repeating ButtonPress
  sim.ctl.update(start + 1500, sim.rig.reading());
  EXPECT_EQ(sim.ctl.getState(), ControllerState::Venting);
  EXPECT_TRUE(sim.rig.vent);
}

TEST(Manual, ReleasingTheOtherDirectionDoesNothing) {
  Sim sim(30.0f);
  sim.ctl.manualAirUp(true, sim.now);
  sim.ctl.manualVent(true, sim.now);   // latest press wins
  sim.ctl.manualAirUp(false, sim.now); // stale release of the first button
  EXPECT_EQ(sim.ctl.getState(), ControllerState::Venting);
  EXPECT_TRUE(sim.rig.vent);
}

TEST(Manual, AirStopsAtMaximumWithOverPressure) {
  Sim sim(49.5f);
  sim.ctl.manualAirUp(true, sim.now);
  sim.ctl.update(sim.now, sim.rig.reading());  // hose offset pushes the live reading past 50
  EXPECT_EQ(sim.ctl.getState(), ControllerState::Error);
  EXPECT_EQ(sim.ctl.getError(), ErrorCode::OverPressure);
  EXPECT_FALSE(sim.rig.compressor);
}

TEST(Manual, IgnoredWhileInError) {
  Sim sim(49.5f);
  sim.ctl.manualAirUp(true, sim.now);
  sim.ctl.update(sim.now, sim.rig.reading());
  sim.ctl.manualAirUp(true, sim.now);
  EXPECT_EQ(sim.ctl.getState(), ControllerState::Error);
  EXPECT_FALSE(sim.rig.compressor);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
