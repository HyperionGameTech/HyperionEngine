#ifndef HYPERION_PROFILE_H
#define HYPERION_PROFILE_H

#include <Types.hpp>

#include <vector>
#include <functional>
#include <type_traits>

namespace hyperion {

class Profile
{
public:
    using ProfileFunction = std::add_pointer_t<void()>;

    static std::vector<double> RunInterleved(std::vector<Profile> &&, SizeType runs_per = 5, SizeType num_iterations = 100, SizeType runs_per_iteration = 100);
    
    Profile(ProfileFunction &&lambda)
        : m_lambda(std::forward<ProfileFunction>(lambda)),
          m_result(0.0),
          m_iteration(0)
    {
    }

    Profile(Profile &&other) noexcept = default;
    Profile &operator=(Profile &&other) noexcept = default;
    Profile(const Profile &other) = default;
    Profile &operator=(const Profile &other) = default;

    ~Profile() = default;
    
    Profile &Run(SizeType num_iterations = 100, SizeType runs_per_iteration = 100);

    double GetResult() const { return m_iteration == 0 ? 0.0 : m_result / static_cast<double>(m_iteration); }

    Profile &Reset()
    {
        m_result = 0.0;
        m_iteration = 0;

        return *this;
    }

private:
    ProfileFunction m_lambda;
    double m_result;
    SizeType m_iteration;
};

} // namespace hyperion

#endif