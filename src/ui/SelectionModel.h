#pragma once

#include <vector>

//==============================================================================
/** The selected bands (1-based) and which one is primary (shown in the band panel). */
class SelectionModel
{
public:
    void select (int band);                       // only this band
    void toggle (int band);                       // add or remove; an added band becomes primary
    void set (std::vector<int> bands, int primary);
    void add (const std::vector<int>& bands);
    void remove (int band);
    void clear();

    bool contains (int band) const noexcept;
    bool isEmpty() const noexcept { return selected.empty(); }
    const std::vector<int>& getSelected() const noexcept { return selected; }   // ascending
    int getPrimary() const noexcept { return primary; }                        // 0 when empty

private:
    std::vector<int> selected;
    int primary = 0;
};
