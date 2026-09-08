#ifndef APPLICATIONSECURITYPOLICY_H
#define APPLICATIONSECURITYPOLICY_H

class ApplicationSecurityPolicy final
{
  public:
    constexpr ApplicationSecurityPolicy(bool elevated, bool portablePackage) noexcept
        : m_elevated(elevated)
        , m_portablePackage(portablePackage)
    {
    }

    [[nodiscard]] constexpr bool isElevated() const noexcept { return m_elevated; }
    [[nodiscard]] constexpr bool isPortablePackage() const noexcept { return m_portablePackage; }

    [[nodiscard]] constexpr bool allowsUserControlledLogFile() const noexcept { return !m_elevated; }
    [[nodiscard]] constexpr bool allowsProfileProgramExecution() const noexcept { return !m_elevated; }
    [[nodiscard]] constexpr bool allowsProfileMigrationWriteback() const noexcept { return !m_elevated; }
    [[nodiscard]] constexpr bool allowsElevationRequest() const noexcept { return !m_portablePackage; }
    [[nodiscard]] constexpr bool mustRefuseStartup() const noexcept { return m_elevated && m_portablePackage; }

    [[nodiscard]] static const ApplicationSecurityPolicy &current() noexcept;

  private:
    bool m_elevated;
    bool m_portablePackage;
};

#endif // APPLICATIONSECURITYPOLICY_H
