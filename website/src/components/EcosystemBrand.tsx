import { css } from '@relicloops/cathode';

interface EcosystemBrandProps {
  size?: 'sm' | 'md' | 'lg';
}

export const EcosystemBrand = ( { size = 'md' }: EcosystemBrandProps ) => {

  /* fonts */
  css( './css/fonts/electrolize.css' );
  /* component styles */
  css( './css/components/ecosystem-brand.css' );

  return (
    <span class={`ecosystem-brand ecosystem-brand--${size}`}>
      <span class="ecosystem-brand__org">relicloops</span>
      <span class="ecosystem-brand__sep">/</span>
      <span class="ecosystem-brand__label">ecosystem</span>
    </span>
  );
};
