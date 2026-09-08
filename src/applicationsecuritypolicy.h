#ifndef APPLICATIONSECURITYPOLICY_H
#define APPLICATIONSECURITYPOLICY_H

class ApplicationSecurityPolicy final
{
  public:
    enum class ElevationState
    {
        NotElevated,
        Elevated,
        Unknown,
    };

    constexpr ApplicationSecurityPolicy(ElevationState elevationState, bool portablePackage) noexcept
        : m_elevationState(elevationState)
        , m_portablePackage(portablePackage)
    {
    }

    [[nodiscard]] constexpr bool isElevated() const noexcept { return m_elevationState == ElevationState::Elevated; }
    [[nodiscard]] constexpr bool hasReliableElevationState() const noexcept
    {
        return m_elevationState != ElevationState::Unknown;
    }
    [[nodiscard]] constexpr bool isPortablePackage() const noexcept { return m_portablePackage; }

    [[nodiscard]] constexpr bool allowsUserControlledLogFile() const noexcept
    {
        return m_elevationState == ElevationState::NotElevated;
    }
    [[nodiscard]] constexpr bool allowsProfileProgramExecution() const noexcept
    {
        return m_elevationState == ElevationState::NotElevated;
    }
    [[nodiscard]] constexpr bool allowsProfileMigrationWriteback() const noexcept
    {
        return m_elevationState == ElevationState::NotElevated;
    }
    [[nodiscard]] constexpr bool allowsElevationRequest() const noexcept { return false; }
    [[nodiscard]] constexpr bool mustRefuseStartup() const noexcept
    {
        return m_elevationState != ElevationState::NotElevated;
    }

    [[nodiscard]] static const ApplicationSecurityPolicy &current() noexcept;

  private:
    ElevationState m_elevationState;
    bool m_portablePackage;
};

#endif // APPLICATIONSECURITYPOLICY_H
