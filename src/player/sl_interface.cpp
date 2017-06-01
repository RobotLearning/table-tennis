/**
 *
 * @file sl_interface.cpp
 *
 * @brief Interface of the Player class to the SL real-time simulator and to
 * the robot.
 *
 */

#include <boost/program_options.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <iterator>
#include <armadillo>
#include <cmath>
#include <sys/time.h>
#include "kalman.h"
#include "player.hpp"
#include "tabletennis.h"

using namespace arma;

/* The data structures from SL */
/**
 * @brief (actual) joint space state for each DOF
 */
struct SL_Jstate {
	double   th;   /*!< theta */
	double   thd;  /*!< theta-dot */
	double   thdd; /*!< theta-dot-dot */
	double   ufb;  /*!< feedback portion of command */
	double   u;    /*!< torque command */
	double   load; /*!< sensed torque */
};
/**
 * @brief (desired) joint space state commands for each DOF
 */
struct SL_DJstate { /*!< desired values for controller */
	double   th;   /*!< theta */
	double   thd;  /*!< theta-dot */
	double   thdd; /*!< theta-dot-dot */
	double   uff;  /*!< feedforward torque command */
	double   uex;  /*!< externally imposed torque */
};

/**
 * @brief (actual) Cartesian state
 */
struct SL_Cstate {
	double   x[NCART+1];    /*!< Position [x,y,z] */
	double   xd[NCART+1];   /*!< Velocity */
	double   xdd[NCART+1];  /*!< Acceleration */
};

/**
 * @brief Vision blob coming from SL.
 */
struct SL_VisionBlob {
	char       status;
	SL_Cstate  blob;
};

player_flags flags; //! global structure for setting Player options

#include "sl_interface.h"

/**
 * @brief Set algorithm to initialize Player with.
 *
 * @param alg_num Select between three algorithms: VHP/FIXED/LAZY.
 */
void set_algorithm(const int alg_num) {

	switch (alg_num) {
		case 0:
			std::cout << "Setting to FOCUSED player..." << std::endl;
			flags.alg = FOCUS;
			break;
		case 1:
			std::cout << "Setting to LAZY player..." << std::endl;
			flags.alg = LAZY;
			break;
		case 2:
			std::cout << "Setting to VHP player..." << std::endl;
			flags.alg = VHP;
			break;
		default:
			flags.alg = FOCUS;
	}
}

// A helper function to simplify the main part.
//template<class T>
//std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
//    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
//    return os;
//}

/**
 * @brief Set algorithm and options to initialize Player with.
 *
 * The global variable flags is set here and
 * the play() function will use it to initialize the Player class.
 *
 */
void load_options() {

	namespace po = boost::program_options;
	using namespace std;

	flags.reset = true;
	string home = std::getenv("HOME");
	string config_file = home + "/polyoptim/" + "player.cfg";
	int alg_num;

    try {
		// Declare a group of options that will be
		// allowed in config file
		po::options_description config("Configuration");
		config.add_options()
		    ("outlier_detection", po::value<bool>(&flags.outlier_detection)->default_value(true),
			      "OUTLIER DETECTION FOR REAL ROBOT!")
			("algorithm", po::value<int>(&alg_num)->default_value(0),
				  "optimization method")
			("mpc", po::value<bool>(&flags.mpc)->default_value(false),
				 "corrections (MPC)")
			("spin", po::value<bool>(&flags.spin)->default_value(false),
						 "apply spin model")
			("verbose", po::value<int>(&flags.verbosity)->default_value(1),
		         "verbosity level")
		    ("save_data", po::value<bool>(&flags.save)->default_value(false),
		         "saving robot/ball data")
		    ("ball_land_des_x_offset", po::value<double>(&flags.ball_land_des_offset[0]),
		    	 "ball land x offset")
			("ball_land_des_y_offset", po::value<double>(&flags.ball_land_des_offset[1]),
				 "ball land y offset")
		    ("time_land_des", po::value<double>(&flags.time_land_des),
		    	 "time land des")
			("start_optim_offset", po::value<double>(&flags.optim_offset),
				 "start optim offset")
			("time2return", po::value<double>(&flags.time2return),
						 "time to return to start posture")
			("freq_mpc", po::value<int>(&flags.freq_mpc), "frequency of updates")
		    ("min_obs", po::value<int>(&flags.min_obs), "minimum obs to start filter")
		    ("std_noise", po::value<double>(&flags.std_noise), "std of filter obs noise")
		    ("std_model", po::value<double>(&flags.std_model), "std of filter process noise")
		    ("mu_est_mult", po::value<double>(&flags.mult_mu_init), "initial filter mean est. multiplier")
		    ("P_est_mult", po::value<double>(&flags.mult_p_init), "initial filter var est. multiplier")
		    ("t_reset_threshold", po::value<double>(&flags.t_reset_thresh), "filter reset threshold time")
		    ("VHPY", po::value<double>(&flags.VHPY), "location of VHP");
        po::variables_map vm;
        ifstream ifs(config_file.c_str());
        if (!ifs) {
            cout << "can not open config file: " << config_file << "\n";
        }
        else {
            po::store(parse_config_file(ifs, config), vm);
            notify(vm);
        }
    }
    catch(exception& e) {
        cout << e.what() << "\n";
    }
    set_algorithm(alg_num);
    flags.detach = true; // always detached in SL/REAL ROBOT!
}

/*
 *
 * Checks for the validity of blob ball data using obvious table tennis checks.
 * Returns TRUE if valid.
 *
 * Only checks for obvious outliers. Does not use uncertainty estimates
 * to assess validity so do not rely on it as the sole source of outlier detection!
 *
 *
 * @param blob Blob structure from SL (one-indexed). Contains a status boolean
 * variable and cartesian coordinates (indices one-to-three).
 * @param verbose If verbose is TRUE, then detecting obvious outliers will
 * print to standard output.
 * @return valid If ball is valid (status is TRUE and not an obvious outlier)
 * return true.
 */
static bool check_blob_validity(const SL_VisionBlob & blob, bool verbose) {

	bool valid = true;
	static double last_blob[NCART+1];
	static double zMax = 0.5;
	static double zMin = floor_level - table_height;
	static double xMax = table_width/2.0;
	static double yMax = 0.5;
	static double yMin = dist_to_table - table_length - 1.0;
	static double yCenter = dist_to_table - table_length/2.0;

	if (blob.status == false) {
		if (verbose)
			printf("BLOB NOT VALID! Ball status is false!\n");
		valid = false;
	}
	else if (blob.blob.x[3] > zMax) {
		if (verbose)
			printf("BLOB NOT VALID! Ball is above zMax = 0.5!\n");
		valid = false;
	}
	else if (last_blob[2] < yCenter && blob.blob.x[2] > dist_to_table) {
		if (verbose)
			printf("BLOB NOT VALID! Ball suddenly jumped in Y!\n");
		valid = false;
	}
	// on the table blob should not appear under the table
	else if (fabs(blob.blob.x[1]) < xMax && fabs(blob.blob.x[2] - yCenter) < table_length/2.0
			&& blob.blob.x[3] < zMin) {
		if (verbose)
			printf("BLOB NOT VALID! Ball appears under the table!\n");
		valid = false;
	}
	last_blob[1] = blob.blob.x[1];
	last_blob[2] = blob.blob.x[2];
	last_blob[3] = blob.blob.x[3];
	return valid;
}

/*
 *
 * Fusing the blobs
 * If both blobs are valid blob3 is preferred
 * Only updates if the blobs are valid, i.e. not obvious outliers
 *
 */
static bool fuse_blobs(const SL_VisionBlob blobs[4], vec3 & obs) {

	static bool status = false;

	// if ball is detected reliably
	// Here we hope to avoid outliers and prefer the blob3 over blob1
	if (check_blob_validity(blobs[3],flags.verbosity) ||
			check_blob_validity(blobs[1],flags.verbosity)) {
		status = true;
		if (blobs[3].status) {
			for (int i = X; i <= Z; i++)
				obs(i) = blobs[3].blob.x[i+1];
		}
		else {
			for (int i = X; i <= Z; i++)
				obs(i) = blobs[1].blob.x[i+1];
		}
	}
	return status;
}

/**
 * @brief Interface to the PLAYER class that generates desired hitting trajectories.
 *
 * First initializes the player according to the pre-set options
 * and then starts calling play() interface function. Must be called every DT ms.
 *
 *
 * @param joint_state Actual joint positions, velocities, accelerations.
 * @param blobs Two ball 3d-positions from 4-cameras are stored in blobs[1] and blobs[3]
 * @param joint_des_state Desired joint position, velocity and acceleration commands.
 */
void play(const SL_Jstate joint_state[NDOF+1],
		  const SL_VisionBlob blobs[4],
		  SL_DJstate joint_des_state[NDOF+1]) {

	static vec7 q0;
	static vec3 ball_obs;
	static joint qact;
	static joint qdes;
	static Player *robot = nullptr; // centered player
	static EKF filter = init_filter(0.3,0.001,flags.spin);

	if (flags.reset) {
		for (int i = 0; i < NDOF; i++) {
			qdes.q(i) = q0(i) = joint_state[i+1].th;
			qdes.qd(i) = 0.0;
			qdes.qdd(i) = 0.0;
		}
		filter = init_filter(0.3,0.001,flags.spin);
		delete robot;
		robot = new Player(q0,filter,flags);
		flags.reset = false;
	}
	else {
		for (int i = 0; i < NDOF; i++) {
			qact.q(i) = joint_state[i+1].th;
			qact.qd(i) = joint_state[i+1].thd;
			qact.qdd(i) = joint_state[i+1].thdd;
		}
		fuse_blobs(blobs,ball_obs);
		robot->play(qact,ball_obs,qdes);
		save_data(qact,qdes,blobs,ball_obs,filter);
	}

	// update desired joint state
	for (int i = 0; i < NDOF; i++) {
		joint_des_state[i+1].th = qdes.q(i);
		joint_des_state[i+1].thd = qdes.qd(i);
		joint_des_state[i+1].thdd = qdes.qdd(i);
	}

}

/*
 *
 * Saves actual and desired joints to one file if save flag is set to TRUE
 * and the ball observations and estimated ball state one another
 *
 * TODO: no need to open close each time!
 *
 */
static void save_data(const joint & qact, const joint & qdes,
		       const SL_VisionBlob blobs[4], const vec3 & ball_obs, const KF & filter) {

	static rowvec ball_full;
    static rowvec rq; // joints in rowvector form
	static std::string home = std::getenv("HOME");
	static std::string joint_file = home + "/polyoptim/joints.txt";
	static std::string ball_file = home + "/polyoptim/balls.txt";
	static std::ofstream stream_joints;
	static std::ofstream stream_balls;
	static vec6 ball_est = zeros<vec>(6);

	if (flags.save) {
		try {
			ball_est = filter.get_mean();
		}
		catch (const char * exception) {
			// do nothing
		}

		rq = join_horiz(qdes.q.t(),qact.q.t());
		//qdes_full = join_horiz(join_horiz(qdes.q.t(),qdes.qd.t()),qdes.qdd.t());
		//qact_full = join_horiz(join_horiz(qact.q.t(),qact.qd.t()),qact.qdd.t());
		stream_joints.open(joint_file,ios::out | ios::app);
		if (stream_joints.is_open()) {
			stream_joints << rq; //qdes_full << qact_full << endr;
		}

		stream_balls.open(ball_file,ios::out | ios::app);
		ball_full << 1 << ((int)blobs[1].status)
				  << blobs[1].blob.x[1] << blobs[1].blob.x[2] << blobs[1].blob.x[3]
				  << 3 << ((int)blobs[3].status)
				  << blobs[3].blob.x[1] << blobs[3].blob.x[2] << blobs[3].blob.x[3] << endr;
		ball_full = join_horiz(join_horiz(ball_full,ball_obs.t()),ball_est.t());
		if (stream_balls.is_open()) {
			stream_balls << ball_full;
		}
		//stream_joints.close();
		stream_balls.close();
	}
}

/**
 * @brief  CHEAT with exact knowledge of ball state.
 *
 * Interface to the PLAYER class that generates desired hitting trajectories.
 * First initializes the player and then starts calling cheat() interface function.
 *
 * @param joint_state Actual joint positions, velocities, accelerations.
 * @param sim_ball_state Exact simulated ball state (positions and velocities).
 * @param joint_des_state Desired joint position, velocity and acceleration commands.
 */
void cheat(const SL_Jstate joint_state[NDOF+1],
		  const SL_Cstate sim_ball_state,
		  SL_DJstate joint_des_state[NDOF+1]) {

	static vec7 q0;
	static vec6 ball_state;
	static joint qact;
	static joint qdes;
	static Player *cp; // centered player
	static EKF filter = init_filter();

	if (flags.reset) {
		for (int i = 0; i < NDOF; i++) {
			qdes.q(i) = q0(i) = joint_state[i+1].th;
			qdes.qd(i) = 0.0;
			qdes.qdd(i) = 0.0;
		}
		cp = new Player(q0,filter,flags);
		flags.reset = false;
	}
	else {
		for (int i = 0; i < NDOF; i++) {
			qact.q(i) = joint_state[i+1].th;
			qact.qd(i) = joint_state[i+1].thd;
			qact.qdd(i) = joint_state[i+1].thdd;
		}
		for (int i = 0; i < NCART; i++) {
			ball_state(i) = sim_ball_state.x[i+1];
			ball_state(i+NCART) = sim_ball_state.xd[i+1];
		}
		cp->cheat(qact,ball_state,qdes);
	}

	// update desired joint state
	for (int i = 0; i < NDOF; i++) {
		joint_des_state[i+1].th = qdes.q(i);
		joint_des_state[i+1].thd = qdes.qd(i);
		joint_des_state[i+1].thdd = qdes.qdd(i);
	}
}
