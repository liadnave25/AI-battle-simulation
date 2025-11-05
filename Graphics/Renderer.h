#pragma once
#include <vector>
#include <string>

#include "Definitions.h"
#include "Grid.h"
#include "Units.h"
#include "SecurityMap.h"
#include "Visibility.h"

namespace Painting {

    void SetHudEnabled(bool on);  

    void RenderInit(int windowW, int windowH);

    void RenderFrame(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units);

    void RenderFrameWithHUD(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        const std::vector<std::string>& hudLines);

    void RenderFrameWithSecurity(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        const Simulation::SecurityMap& smap,
        const std::vector<std::string>& hudLines);

    void RenderFrameWithVisibility(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        const AI::Visibility::BArray& vis,
        const std::vector<std::string>& hudLines);

    using OverlayDrawFn = void(*)();

    void RenderFrame_Overlay(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        OverlayDrawFn overlay);

    void RenderFrameWithHUD_Overlay(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        const std::vector<std::string>& hudLines,
        OverlayDrawFn overlay);

    void RenderFrameWithSecurity_Overlay(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        const Simulation::SecurityMap& smap,
        const std::vector<std::string>& hudLines,
        OverlayDrawFn overlay);

    void RenderFrameWithVisibility_Overlay(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        const AI::Visibility::BArray& vis,
        const std::vector<std::string>& hudLines,
        OverlayDrawFn overlay);
}
