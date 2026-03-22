import { css } from '@relicloops/cathode';

import { BackToTop } from '../components/BackToTop';
import { Footer } from '../components/Footer';
import { Header } from '../components/Header';
import { Hero } from '../components/index/Hero';

export const Index = () => {
  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* component styles */
  css( './css/pages/index.css' );

  return (
    <>
      <Hero />
      <Header />
      <Footer />
      <BackToTop />
    </>
  );
};
