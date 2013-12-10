/* Copyright (c) 2012-2013 Stanislaw Halik <sthalik@misaki.pl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

/*
 * this file appeared originally in facetracknoir, was rewritten completely
 * following opentrack fork.
 *
 * originally written by Wim Vriend.
 */

#include "tracker.h"
#include "facetracknoir.h"
#include <opencv2/core/core.hpp>
#include <cmath>
#include <algorithm>

#if defined(_WIN32)
#   include <windows.h>
#endif

Tracker::Tracker( FaceTrackNoIR *parent ) :
    should_quit(false),
    do_center(false),
    enabled(true),
    compensate(true)
{
    mainApp = parent;
}

Tracker::~Tracker()
{
}

static void get_curve(double pos, double& out, THeadPoseDOF& axis) {
    bool altp = (pos < 0) && axis.altp;
    if (altp) {
        out = axis.invert * axis.curveAlt.getValue(pos);
        axis.curve.setTrackingActive( false );
        axis.curveAlt.setTrackingActive( true );
    }
    else {
        out = axis.invert * axis.curve.getValue(pos);
        axis.curve.setTrackingActive( true );
        axis.curveAlt.setTrackingActive( false );
    }
    out += axis.zero;
}

static void t_compensate(double* input, double* output)
{
    const auto H = input[Yaw] * M_PI / 180;
    const auto P = input[Pitch] * M_PI / 180;
    const auto B = input[Roll] * M_PI / 180;

    const auto cosH = cos(H);
    const auto sinH = sin(H);
    const auto cosP = cos(P);
    const auto sinP = sin(P);
    const auto cosB = cos(B);
    const auto sinB = sin(B);

    double foo[] = {
        cosH * cosB - sinH * sinP * sinB,
        - sinB * cosP,
        sinH * cosB + cosH * sinP * sinB,
        cosH * sinB + sinH * sinP * cosB,
        cosB * cosP,
        sinB * sinH - cosH * sinP * cosB,
        - sinH * cosP,
        - sinP,
        cosH * cosP,
    };

    cv::Mat rmat(3, 3, CV_64F, foo);
    const cv::Mat tvec(3, 1, CV_64F, input);
    cv::Mat ret = rmat * tvec;

    for (int i = 0; i < 3; i++)
        output[i] = ret.at<double>(i);
}

/** QThread run method @override **/
void Tracker::run() {
    T6DOF offset_camera;

    double newpose[6] = {0};
    int sleep_ms = 15;

    if (Libraries->pTracker)
        sleep_ms = std::min(sleep_ms, 1000 / Libraries->pTracker->preferredHz());

    if (Libraries->pSecondTracker)
        sleep_ms = std::min(sleep_ms, 1000 / Libraries->pSecondTracker->preferredHz());

    qDebug() << "tracker Hz:" << 1000 / sleep_ms;

#if defined(_WIN32)
    (void) timeBeginPeriod(1);
#endif

    for (;;)
    {
        if (should_quit)
            break;

        if (Libraries->pSecondTracker) {
            Libraries->pSecondTracker->GiveHeadPoseData(newpose);
        }

        if (Libraries->pTracker) {
            Libraries->pTracker->GiveHeadPoseData(newpose);
        }

        {
            QMutexLocker foo(&mtx);

            for (int i = 0; i < 6; i++)
                mainApp->axis(i).headPos = newpose[i];

            if (do_center)  {
                for (int i = 0; i < 6; i++)
                    offset_camera.axes[i] = mainApp->axis(i).headPos;

                do_center = false;

                if (Libraries->pFilter)
                    Libraries->pFilter->Initialize();
            }

            T6DOF target_camera, target_camera2, new_camera;

            if (enabled)
            {
                for (int i = 0; i < 6; i++)
                    target_camera.axes[i] = mainApp->axis(i).headPos;

                target_camera2 = target_camera - offset_camera;
            }

            if (Libraries->pFilter) {
                Libraries->pFilter->FilterHeadPoseData(target_camera2.axes, new_camera.axes);
            } else {
                new_camera = target_camera2;
            }

            for (int i = 0; i < 6; i++) {
                get_curve(new_camera.axes[i], output_camera.axes[i], mainApp->axis(i));
            }

            if (compensate)
                t_compensate(output_camera.axes, output_camera.axes);

            if (Libraries->pProtocol) {
                Libraries->pProtocol->sendHeadposeToGame( output_camera.axes );	// degrees & centimeters
            }
        }

        msleep(sleep_ms);
    }
#if defined(_WIN32)
    (void) timeEndPeriod(1);
#endif

    for (int i = 0; i < 6; i++)
    {
        mainApp->axis(i).curve.setTrackingActive(false);
        mainApp->axis(i).curveAlt.setTrackingActive(false);
    }
}

void Tracker::getHeadPose( double *data ) {
    QMutexLocker foo(&mtx);
    for (int i = 0; i < 6; i++)
    {
        data[i] = mainApp->axis(i).headPos;
    }
}

void Tracker::getOutputHeadPose( double *data ) {
    QMutexLocker foo(&mtx);
    for (int i = 0; i < 6; i++)
        data[i] = output_camera.axes[i];
}

void Tracker::setInvertAxis(Axis axis, bool invert) { mainApp->axis(axis).invert = invert? -1.0 : 1.0; }
