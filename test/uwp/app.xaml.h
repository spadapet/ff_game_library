#pragma once

#include "App.g.h"

namespace test_uwp
{
    ref class app sealed
    {
    protected:
        virtual void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ args) override;

    internal:
        app();

    private:
    };
}
