# ifndef Pb_MHD
# define Pb_MHD

#include <Pb_Hydraulique.h>
#include <MHD_std.h>

/*! @brief blabla
*/

class Pb_MHD: public Pb_Hydraulique
{
  Declare_instanciable(Pb_MHD);

public:

  int nombre_d_equations() const override { return 1 + eq_opt_.size(); }
  const Equation_base& equation(int) const override;
  Equation_base& equation(int) override;
  void associer_milieu_base(const Milieu_base&) override;

protected:

  MHD_std equation_magnetique_;
  const double eta_ ; // magnetic diffusivity
  double Re_m ; // magnetic Reynolds number
  
}

#endif
