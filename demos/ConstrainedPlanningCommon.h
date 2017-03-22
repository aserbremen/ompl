/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2014, Rice University
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Rice University nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Zachary Kingston */

#include <fstream>
#include <thread>

#include <ompl/base/ScopedState.h>
#include <ompl/base/Constraint.h>
#include <ompl/base/StateSpace.h>
#include <ompl/base/spaces/AtlasChart.h>
#include <ompl/base/spaces/AtlasStateSpace.h>
#include <ompl/base/spaces/ProjectedStateSpace.h>
#include <ompl/geometric/SimpleSetup.h>
#include <ompl/geometric/PathGeometric.h>
#include <ompl/geometric/planners/est/BiEST.h>
#include <ompl/geometric/planners/est/EST.h>
#include <ompl/geometric/planners/est/ProjEST.h>
#include <ompl/geometric/planners/kpiece/BKPIECE1.h>
#include <ompl/geometric/planners/kpiece/KPIECE1.h>
#include <ompl/geometric/planners/kpiece/LBKPIECE1.h>
#include <ompl/geometric/planners/pdst/PDST.h>
#include <ompl/geometric/planners/prm/PRM.h>
#include <ompl/geometric/planners/prm/PRMstar.h>
#include <ompl/geometric/planners/prm/SPARS.h>
#include <ompl/geometric/planners/prm/SPARStwo.h>
#include <ompl/geometric/planners/rrt/LBTRRT.h>
#include <ompl/geometric/planners/rrt/LazyRRT.h>
#include <ompl/geometric/planners/rrt/RRT.h>
#include <ompl/geometric/planners/rrt/RRTConnect.h>
#include <ompl/geometric/planners/rrt/RRTstar.h>
#include <ompl/geometric/planners/rrt/TRRT.h>
#include <ompl/geometric/planners/sbl/SBL.h>
#include <ompl/geometric/planners/stride/STRIDE.h>

/** Simple manifold example: the unit sphere. */
class SphereConstraint : public ompl::base::Constraint
{
public:
    SphereConstraint(const ompl::base::StateSpace *space) : ompl::base::Constraint(space, 2)
    {
    }

    void function(const Eigen::VectorXd &x, Eigen::Ref<Eigen::VectorXd> out) const
    {
        out[0] = x.norm() - 1;
    }

    void jacobian(const Eigen::VectorXd &x, Eigen::Ref<Eigen::MatrixXd> out) const
    {
        out = x.transpose().normalized();
    }
};

/** Simple manifold example: the xy plane. */
class PlaneConstraint : public ompl::base::Constraint
{
public:
    PlaneConstraint(const ompl::base::StateSpace *space) : ompl::base::Constraint(space, 2)
    {
    }

    void function(const Eigen::VectorXd &x, Eigen::Ref<Eigen::VectorXd> out) const
    {
        out[0] = x[2];
    }

    void jacobian(const Eigen::VectorXd &x, Eigen::Ref<Eigen::MatrixXd> out) const
    {
        out(0, 0) = 0;
        out(0, 1) = 0;
        out(0, 2) = 1;
    }
};

/** Torus manifold. */
class TorusConstraint : public ompl::base::Constraint
{
public:
    const double R1;
    const double R2;

    TorusConstraint(const ompl::base::StateSpace *space, const double r1, const double r2)
      : ompl::base::Constraint(space, 2), R1(r1), R2(r2)
    {
    }

    void function(const Eigen::VectorXd &x, Eigen::Ref<Eigen::VectorXd> out) const
    {
        Eigen::VectorXd c(3);
        c << x[0], x[1], 0;
        out[0] = (x - R1 * c.normalized()).norm() - R2;
    }

    void jacobian(const Eigen::VectorXd &x, Eigen::Ref<Eigen::MatrixXd> out) const
    {
        const double xySquaredNorm = x[0] * x[0] + x[1] * x[1];
        const double xyNorm = std::sqrt(xySquaredNorm);
        const double denom = std::sqrt(x[2] * x[2] + (xyNorm - R1) * (xyNorm - R1));
        const double c = (xyNorm - R1) * (xyNorm * xySquaredNorm) / (xySquaredNorm * xySquaredNorm * denom);
        out(0, 0) = x[0] * c;
        out(0, 1) = x[1] * c;
        out(0, 2) = x[2] / denom;
    }
};

class KleinConstraint : public ompl::base::Constraint
{
public:
    KleinConstraint(const ompl::base::StateSpace *space) : ompl::base::Constraint(space, 2)
    {
    }

    void function(const Eigen::VectorXd &x, Eigen::Ref<Eigen::VectorXd> out) const
    {
        const double p = x.squaredNorm() + 2 * x[1] - 1;
        const double n = x.squaredNorm() - 2 * x[1] - 1;
        const double u = n * n - 8 * x[2] * x[2];

        out[0] = p * u + 16 * x[0] * x[1] * n;
    }

    void jacobian(const Eigen::VectorXd &x, Eigen::Ref<Eigen::MatrixXd> out) const
    {
        const double p = x.squaredNorm() + 2 * x[1] - 1;
        const double n = x.squaredNorm() - 2 * x[1] - 1;
        const double u = n * n - 8 * x[2] * x[2];

        out(0, 0) = 32 * x[0] * x[0] * x[1] + 16 * x[1] * n + 4 * x[0] * n * p + 2 * x[0] * u;
        out(0, 1) = 32 * x[0] * x[1] * (x[1] - 1) + 16 * x[0] * n + 4 * (x[1] - 1) * n * p + 2 * (x[1] + 1) * u;
        out(0, 2) = 2 * x[2] * (16 * x[0] * x[1] + 2 * p * (n - 4) + u);
    }
};

/** Kinematic chain manifold. */
class ChainConstraint : public ompl::base::Constraint
{
public:
    const unsigned int workspaceDim;  // Workspace dimension.
    const unsigned int nLinks;        // Number of chain links.
    const double linkLength;          // Length of one link.
    // Radius of the sphere that the end effector is constrained to.
    const double sphereRadius;
    // Joints are cubes, and the collision checker makes sure they don't
    // intersect. This controls how big they are.
    const double jointSize;
    // Number of additional constraints to apply.
    // 1: end effector sphere
    // 2: joints 0 and 1 have same z-value
    // 3: joints 1 and 2 have same x-value
    // 4: joints 2 and 3 have same y-value
    // 5: joints 0 and 4 have same y value
    const unsigned int extraConstraints;

    ChainConstraint(ompl::base::StateSpace *space, unsigned int dim, unsigned int links,
                    unsigned int extraConstraints = 1)
      : ompl::base::Constraint(space, (dim - 1) * links - extraConstraints)
      , workspaceDim(dim)
      , nLinks(links)
      , linkLength(1.0)
      , sphereRadius(3.0)
      , jointSize(0.2)
      , extraConstraints(extraConstraints)
    {
        std::cout << "Ambient dimension: " << getAmbientDimension() << "\n";
        std::cout << "Manifold dimension: " << getManifoldDimension() << "\n";
    }

    void function(const Eigen::VectorXd &x, Eigen::Ref<Eigen::VectorXd> out) const
    {
        // Consecutive joints must be a fixed distance apart.
        Eigen::VectorXd joint1 = Eigen::VectorXd::Zero(workspaceDim);
        for (unsigned int i = 0; i < nLinks; i++)
        {
            const Eigen::VectorXd joint2 = x.segment(workspaceDim * i, workspaceDim);
            out[i] = (joint1 - joint2).norm() - linkLength;
            joint1 = joint2;
        }

        // End effector must lie on a sphere.
        if (extraConstraints >= 1)
            out[nLinks] = x.tail(workspaceDim).norm() - sphereRadius;
        // Joints 0 and 1 must have same z-value.
        if (extraConstraints >= 2)
            out[nLinks + 1] = x[0 * workspaceDim + 2] - x[1 * workspaceDim + 2];
        // Joints 1 and 2 must have same x-value.
        if (extraConstraints >= 3)
            out[nLinks + 2] = x[1 * workspaceDim + 0] - x[2 * workspaceDim + 0];
        // Joints 2 and 3 must have the same y-value.
        if (extraConstraints >= 4)
            out[nLinks + 3] = x[2 * workspaceDim + 1] - x[3 * workspaceDim + 1];
        // Joints 0 and 4 joints have same y-value.
        if (extraConstraints >= 5)
            out[nLinks + 4] = x[0 * workspaceDim + 1] - x[4 * workspaceDim + 1];
    }

    void jacobian(const Eigen::VectorXd &x, Eigen::Ref<Eigen::MatrixXd> out) const
    {
        out.setZero();
        Eigen::VectorXd plus(workspaceDim * (nLinks + 1));
        plus.head(workspaceDim * nLinks) = x;
        plus.tail(workspaceDim) = Eigen::VectorXd::Zero(workspaceDim);
        Eigen::VectorXd minus(workspaceDim * (nLinks + 1));
        minus.head(workspaceDim) = Eigen::VectorXd::Zero(workspaceDim);
        minus.tail(workspaceDim * nLinks) = x;
        const Eigen::VectorXd diagonal = plus - minus;
        for (unsigned int i = 0; i < nLinks; i++)
            out.row(i).segment(workspaceDim * i, workspaceDim) =
                diagonal.segment(workspaceDim * i, workspaceDim).normalized();
        out.block(1, 0, nLinks, workspaceDim * (nLinks - 1)) -=
            out.block(1, workspaceDim, nLinks, workspaceDim * (nLinks - 1));

        if (extraConstraints >= 1)
            out.row(nLinks).tail(workspaceDim) = -diagonal.tail(workspaceDim).normalized().transpose();
        if (extraConstraints >= 2)
        {
            out(nLinks + 1, 0 * workspaceDim + 2) = 1;
            out(nLinks + 1, 1 * workspaceDim + 2) = -1;
        }
        if (extraConstraints >= 3)
        {
            out(nLinks + 2, 1 * workspaceDim + 0) = 1;
            out(nLinks + 2, 2 * workspaceDim + 0) = -1;
        }
        if (extraConstraints >= 4)
        {
            out(nLinks + 3, 2 * workspaceDim + 1) = 1;
            out(nLinks + 3, 3 * workspaceDim + 1) = -1;
        }
        if (extraConstraints >= 5)
        {
            out(nLinks + 4, 0 * workspaceDim + 1) = 1;
            out(nLinks + 4, 4 * workspaceDim + 1) = -1;
        }
    }

    /** Joints may not get touch each other. If \a tough == true, then the end
    * effector may not occupy states on the sphere similar to the sphereValid()
    * obstacles. */
    bool isValid(double sleep, const ompl::base::State *state)
    {
        std::this_thread::sleep_for(ompl::time::seconds(sleep));
        Eigen::Ref<const Eigen::VectorXd> x = state->as<ompl::base::AtlasStateSpace::StateType>()->constVectorView();
        for (unsigned int i = 0; i < nLinks - 1; i++)
        {
            if (x.segment(workspaceDim * i, workspaceDim).cwiseAbs().maxCoeff() < jointSize)
                return false;
            for (unsigned int j = i + 1; j < nLinks; j++)
            {
                if ((x.segment(workspaceDim * i, workspaceDim) - x.segment(workspaceDim * j, workspaceDim))
                        .cwiseAbs()
                        .maxCoeff() < jointSize)
                    return false;
            }
        }

        return true;
    }
};

/**
 * State validity checking functions implicitly define the free space where they return true.
 */

/** 3 ring-shaped obstacles on latitudinal lines, with a small gap in each. */
bool sphereValid_helper(const double *x)
{
    if (-0.75 < x[2] && x[2] < -0.60)
    {
        if (-0.05 < x[1] && x[1] < 0.05)
            return x[0] > 0;
        return false;
    }
    else if (-0.1 < x[2] && x[2] < 0.1)
    {
        if (-0.05 < x[0] && x[0] < 0.05)
            return x[1] < 0;
        return false;
    }
    else if (0.60 < x[2] && x[2] < 0.75)
    {
        if (-0.05 < x[1] && x[1] < 0.05)
            return x[0] > 0;
        return false;
    }
    return true;
}

/** Correct path on the sphere must snake around. */
bool sphereValid(double sleep, const ompl::base::State *state)
{
    std::this_thread::sleep_for(ompl::time::seconds(sleep));
    return sphereValid_helper(state->as<ompl::base::RealVectorStateSpace::StateType>()->values);
}

/** Every state is valid. */
bool always(double sleep, const ompl::base::State *)
{
    std::this_thread::sleep_for(ompl::time::seconds(sleep));
    return true;
}

/** States surrounding the goal are invalid, making it unreachable. We can use this to build up an atlas
 * until time runs out, so we can see the big picture. */
// bool unreachable(double sleep, const ompl::base::State *state, const Eigen::VectorXd &goal, const double radius)
// {
//     std::this_thread::sleep_for(ompl::time::seconds(sleep));
//     return std::abs((state->as<ompl::base::AtlasStateSpace::StateType>()->constVectorView() - goal).norm() - radius)
//     >
//            radius - 0.01;
// }

/**
 * Problem initialization functions set the dimension, the manifold, start and goal points \a x and \a y,
 * and the validity checker \a isValid.
 */

ompl::base::Constraint *initPlaneProblem(Eigen::VectorXd &x, Eigen::VectorXd &y,
                                         ompl::base::StateValidityCheckerFn &isValid, double sleep)
{
    const std::size_t dim = 3;

    x = Eigen::VectorXd(dim);
    x << 4, 4, 0;
    y = Eigen::VectorXd(dim);
    y << -4, -4, 0;

    isValid = std::bind(&always, sleep, std::placeholders::_1);

    ompl::base::StateSpace *space = new ompl::base::RealVectorStateSpace(3);
    return new PlaneConstraint(space);
}

/** Initialize the atlas for the sphere problem and store the start and goal vectors. */
ompl::base::Constraint *initSphereProblem(Eigen::VectorXd &x, Eigen::VectorXd &y,
                                          ompl::base::StateValidityCheckerFn &isValid, double sleep)
{
    const std::size_t dim = 3;

    // Start and goal points
    x = Eigen::VectorXd(dim);
    x << 0, 0, -1;
    y = Eigen::VectorXd(dim);
    y << 0, 0, 1;

    // Validity checker
    isValid = std::bind(&sphereValid, sleep, std::placeholders::_1);

    // Atlas initialization (can use numerical methods to compute the Jacobian, but giving an explicit function is
    // faster)
    ompl::base::StateSpace *space = new ompl::base::RealVectorStateSpace(3);
    return new SphereConstraint(space);
}

/** Initialize the atlas for the torus problem and store the start and goal vectors. */
ompl::base::Constraint *initTorusProblem(Eigen::VectorXd &x, Eigen::VectorXd &y,
                                         ompl::base::StateValidityCheckerFn &isValid, double sleep)
{
    const std::size_t dim = 3;

    // Start and goal points
    x = Eigen::VectorXd(dim);
    x << -3, 0, -1;
    y = Eigen::VectorXd(dim);
    y << 3, 0, 1;

    // Validity checker
    isValid = std::bind(&always, sleep, std::placeholders::_1);

    ompl::base::StateSpace *space = new ompl::base::RealVectorStateSpace(3);
    return new TorusConstraint(space, 3, 1);
}

/** Initialize the atlas for the sphere problem and store the start and goal vectors. */
ompl::base::Constraint *initKleinProblem(Eigen::VectorXd &x, Eigen::VectorXd &y,
                                         ompl::base::StateValidityCheckerFn &isValid, double sleep)
{
    const std::size_t dim = 3;

    // Start and goal points
    x = Eigen::VectorXd(dim);
    x << -0.5, -0.25, 0.1892222244330081;
    y = Eigen::VectorXd(dim);
    y << 2.5, -1.5, 1.0221854181962458;

    // Validity checker
    isValid = std::bind(&always, sleep, std::placeholders::_1);

    ompl::base::StateSpace *space = new ompl::base::RealVectorStateSpace(3);
    return new KleinConstraint(space);
}

/** Initialize the atlas for the kinematic chain problem. */
ompl::base::Constraint *initChainProblem(Eigen::VectorXd &x, Eigen::VectorXd &y,
                                         ompl::base::StateValidityCheckerFn &isValid, double sleep)
{
    const std::size_t dim = 3 * 5;

    // Start and goal points (each triple is the 3D location of a joint)
    x = Eigen::VectorXd(dim);
    x << 1, 0, 0, 2, 0, 0, 2, -1, 0, 3, -1, 0, 3, 0, 0;
    y = Eigen::VectorXd(dim);
    y << 0, -1, 0, -1, -1, 0, -1, 0, 0, -2, 0, 0, -3, 0, 0;

    ompl::base::StateSpace *space = new ompl::base::RealVectorStateSpace(dim);
    ChainConstraint *atlas = new ChainConstraint(space, 3, 5);
    isValid = std::bind(&ChainConstraint::isValid, atlas, sleep, std::placeholders::_1);
    return atlas;
}

/** Allocator function for a sampler for the atlas that only returns valid points. */
ompl::base::ValidStateSamplerPtr avssa(const ompl::base::SpaceInformation *si)
{
    return ompl::base::ValidStateSamplerPtr(new ompl::base::AtlasValidStateSampler(si));
}

/** Allocator function for a sampler for the atlas that only returns valid points. */
ompl::base::ValidStateSamplerPtr pvssa(const ompl::base::SpaceInformation *si)
{
    return ompl::base::ValidStateSamplerPtr(new ompl::base::ProjectedValidStateSampler(si));
}

/** Print usage information. */
void printProblems(void)
{
    std::cout << "Available problems:\n";
    std::cout << "    plane sphere torus klein chain\n";
}

/** Print usage information. */
void printPlanners(void)
{
    std::cout << "Available planners:\n";
    std::cout << "    EST RealEST BiRealEST SBL STRIDE\n";
    std::cout << "    RRT RRTintermediate RRTConnect RRTConnectIntermediate RRTstar LazyRRT TRRT\n";
    std::cout << "    LBTRRT KPIECE1 BKPIECE1 LBKPIECE1 PDST\n";
    std::cout << "    PRM PRMstar SPARS SPARStwo\n";
}

/** Initialize the problem specified in the string. */
ompl::base::Constraint *parseProblem(const char *const problem, Eigen::VectorXd &x, Eigen::VectorXd &y,
                                     ompl::base::StateValidityCheckerFn &isValid, double sleep = 0)
{
    if (std::strcmp(problem, "plane") == 0)
        return initPlaneProblem(x, y, isValid, sleep);
    else if (std::strcmp(problem, "sphere") == 0)
        return initSphereProblem(x, y, isValid, sleep);
    else if (std::strcmp(problem, "torus") == 0)
        return initTorusProblem(x, y, isValid, sleep);
    else if (std::strcmp(problem, "klein") == 0)
        return initKleinProblem(x, y, isValid, sleep);
    else if (std::strcmp(problem, "chain") == 0)
        return initChainProblem(x, y, isValid, sleep);
    else
        return NULL;
}

/** Initialize the planner specified in the string. */
ompl::base::Planner *parsePlanner(const char *const planner, const ompl::base::SpaceInformationPtr &si,
                                  const double range)
{
    if (std::strcmp(planner, "EST") == 0)
    {
        ompl::geometric::EST *est = new ompl::geometric::EST(si);
        est->setRange(range);
        return est;
    }
    else if (std::strcmp(planner, "BiEST") == 0)
    {
        ompl::geometric::BiEST *est = new ompl::geometric::BiEST(si);
        est->setRange(range);
        return est;
    }
    else if (std::strcmp(planner, "ProjEST") == 0)
    {
        ompl::geometric::ProjEST *est = new ompl::geometric::ProjEST(si);
        est->setRange(range);
        return est;
    }
    else if (std::strcmp(planner, "RRT") == 0)
    {
        ompl::geometric::RRT *rrt = new ompl::geometric::RRT(si);
        return rrt;
    }
    else if (std::strcmp(planner, "RRTintermediate") == 0)
    {
        ompl::geometric::RRT *rrt = new ompl::geometric::RRT(si, true);
        return rrt;
    }
    else if (std::strcmp(planner, "RRTConnect") == 0)
    {
        ompl::geometric::RRTConnect *rrtconnect = new ompl::geometric::RRTConnect(si);
        return rrtconnect;
    }
    else if (std::strcmp(planner, "RRTConnectIntermediate") == 0)
    {
        ompl::geometric::RRTConnect *rrtconnectintermediate = new ompl::geometric::RRTConnect(si, true);
        return rrtconnectintermediate;
    }
    else if (std::strcmp(planner, "RRTstar") == 0)
    {
        ompl::geometric::RRTstar *rrtstar = new ompl::geometric::RRTstar(si);
        return rrtstar;
    }
    else if (std::strcmp(planner, "LazyRRT") == 0)
    {
        ompl::geometric::LazyRRT *lazyrrt = new ompl::geometric::LazyRRT(si);
        return lazyrrt;
    }
    else if (std::strcmp(planner, "TRRT") == 0)
    {
        ompl::geometric::TRRT *trrt = new ompl::geometric::TRRT(si);
        return trrt;
    }
    else if (std::strcmp(planner, "LBTRRT") == 0)
    {
        ompl::geometric::LBTRRT *lbtrrt = new ompl::geometric::LBTRRT(si);
        return lbtrrt;
    }
    else if (std::strcmp(planner, "KPIECE1") == 0)
    {
        ompl::geometric::KPIECE1 *kpiece1 = new ompl::geometric::KPIECE1(si);
        kpiece1->setRange(range);
        return kpiece1;
    }
    else if (std::strcmp(planner, "BKPIECE1") == 0)
    {
        ompl::geometric::BKPIECE1 *bkpiece1 = new ompl::geometric::BKPIECE1(si);
        bkpiece1->setRange(range);
        return bkpiece1;
    }
    else if (std::strcmp(planner, "LBKPIECE1") == 0)
    {
        ompl::geometric::LBKPIECE1 *lbkpiece1 = new ompl::geometric::LBKPIECE1(si);
        lbkpiece1->setRange(range);
        return lbkpiece1;
    }
    else if (std::strcmp(planner, "PDST") == 0)
        return new ompl::geometric::PDST(si);
    else if (std::strcmp(planner, "PRM") == 0)
        return new ompl::geometric::PRM(si);
    else if (std::strcmp(planner, "PRMstar") == 0)
        return new ompl::geometric::PRMstar(si);
    else if (std::strcmp(planner, "SBL") == 0)
    {
        ompl::geometric::SBL *sbl = new ompl::geometric::SBL(si);
        sbl->setRange(range);
        return sbl;
    }
    else if (std::strcmp(planner, "SPARS") == 0)
        return new ompl::geometric::SPARS(si);
    else if (std::strcmp(planner, "SPARStwo") == 0)
        return new ompl::geometric::SPARStwo(si);
    else if (std::strcmp(planner, "STRIDE") == 0)
    {
        ompl::geometric::STRIDE *stride = new ompl::geometric::STRIDE(si);
        stride->setRange(range);
        return stride;
    }
    else
        return NULL;
}
