let clickCount = 0;

document.addEventListener('DOMContentLoaded', function() {
    const btnClick = document.getElementById('btnClick');
    const counter = document.getElementById('counter');
    const contactForm = document.getElementById('contactForm');

    btnClick.addEventListener('click', function() {
        clickCount++;
        counter.textContent = `点击次数: ${clickCount}`;
        
        if (clickCount % 5 === 0) {
            alert(`恭喜！您已经点击了 ${clickCount} 次！`);
        }
    });

    contactForm.addEventListener('submit', function(event) {
        event.preventDefault();
        
        const name = document.getElementById('name').value;
        const email = document.getElementById('email').value;
        const message = document.getElementById('message').value;
        
        console.log('表单提交:', { name, email, message });
        alert(`感谢您的留言，${name}！我们会尽快回复到 ${email}。`);
        
        contactForm.reset();
    });

    const navLinks = document.querySelectorAll('nav a');
    navLinks.forEach(link => {
        link.addEventListener('click', function(event) {
            event.preventDefault();
            const targetId = this.getAttribute('href').substring(1);
            const targetSection = document.getElementById(targetId);
            
            if (targetSection) {
                targetSection.scrollIntoView({ behavior: 'smooth' });
            }
        });
    });
});