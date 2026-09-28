#include "SelectionModel.h"

#include <algorithm>

void SelectionModel::select (int band)
{
    selected = { band };
    primary = band;
}

void SelectionModel::toggle (int band)
{
    if (contains (band))
    {
        remove (band);
        return;
    }

    selected.insert (std::lower_bound (selected.begin(), selected.end(), band), band);
    primary = band;
}

void SelectionModel::set (std::vector<int> bands, int newPrimary)
{
    std::sort (bands.begin(), bands.end());
    bands.erase (std::unique (bands.begin(), bands.end()), bands.end());
    selected = std::move (bands);
    primary = contains (newPrimary) ? newPrimary : (selected.empty() ? 0 : selected.front());
}

void SelectionModel::add (const std::vector<int>& bands)
{
    for (auto b : bands)
        if (! contains (b))
            selected.insert (std::lower_bound (selected.begin(), selected.end(), b), b);

    if (primary == 0 && ! selected.empty())
        primary = selected.front();
}

void SelectionModel::remove (int band)
{
    selected.erase (std::remove (selected.begin(), selected.end(), band), selected.end());

    if (primary == band)
        primary = selected.empty() ? 0 : selected.front();
}

void SelectionModel::clear()
{
    selected.clear();
    primary = 0;
}

bool SelectionModel::contains (int band) const noexcept
{
    return std::binary_search (selected.begin(), selected.end(), band);
}
